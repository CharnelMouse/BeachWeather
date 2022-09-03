#define OLC_PGE_APPLICATION
#include "olcPixelGameEngine.h"

// fixed by engine
static const int letterSize = 8;

static const int pixels = 2;

// syCellHeight must be multiple of four for crenellations,
// must have space for 2*syKeyHeight + sKeySpacing since is space below floor,
// so syCellHeight = 2*syKeyHeight + sKeySpacing + 2 = 4 * syCrenelHeight,
// sKeySpacing = sxCrenelOffset
// => 2*syKeyHeight + sxCrenelOffset + 2 = 4*syCrenelHeight.
// syCrenelHeight = sxCrenelWidth = 2*sxCrenelOffset
// => 2*syKeyHeight + 2 = 7*sxCrenelOffset,
// i.e. 7*sxCrenelOffset = 2*(syKeyHeight + 1) = balanceMult,
// 2 | sxCrenelOffset, 7 | (syKeyHeight + 1), 14 | balanceMult.

static const int balanceMult = 28;

static const int sxCrenelOffset = balanceMult / 7;

static const int sxKeyWidth = balanceMult / 2 - 1;
static const int syKeyHeight = sxKeyWidth;
static const int sKeySpacing = sxCrenelOffset;

static const int sxCrenelWidth = 2 * sxCrenelOffset; // _cc__cc_
static const int syCrenelHeight = sxCrenelWidth; // square crenels

static const int syHalfcellHeight = 2 * syCrenelHeight;

static const int sxCellWidth = 4 * sxCrenelOffset + 2 * sxCrenelWidth; // _cc__cc_
static const int syCellHeight = 4 * syCrenelHeight;

static const int nxCells = 9;
static const int nyCells = 9;

static const int nxParticles = nxCells * sxCellWidth;
static const int nyParticles = nyCells * syCellHeight;

static const int sxCellsOffset = sxCellWidth; // non-build area on left edge for trees, cliffs etc.

static const int sxScreenWidth = sxCellsOffset + sxCellWidth * nxCells;
static const int syBeachMax = syCellHeight * nyCells;
static const int syBottom = 12;
static const int syScreenHeight = syBeachMax + syCellHeight;

static const int sxMoonStart = sxCellsOffset + sxCellWidth;
static const int sxMoonTarget = sxCellsOffset + sxCellWidth * nxCells / 2;
static const int syMoonStart = syCellHeight;
static const int syMoonTarget = syBeachMax;

static const int sxPlayerWidth = 3 * sxCrenelOffset;
static const int syPlayerHeight = 6 * sxCrenelOffset;
static const float sxPlayerSpeed = float(2 * sxCellWidth);
static const float syPlayerSpeed = float(2 * syCellHeight);
static const float syPlayerFallSpeed = float(4 * syCellHeight);
static const float syPlayerDrownSpeed = float(syCellHeight);

static const int sSunRadius = 2 * sxCrenelWidth;

static const int sxLadderOffset = sxCrenelOffset + sxCrenelWidth - 1;

static const olc::Pixel brown = olc::Pixel(128, 0, 64);

static const float sunburnEventRate = 1.0f / 30.0f;
static const float sunburnTime = 5.0f;

static const float timeToRain = 15.0f;
static const float rainRate = 50.0f;
static const float rainFallSpeed = 2.0f;

static const float windEventRate = 1.0f / 10.0f;
static const float windEventDuration = 4.0f;
static const float windSpeed = 1.0f;
static const float wxMaxRainX = 1.0f + windSpeed / rainFallSpeed;
static const float wxMinRainX = -windSpeed / rainFallSpeed;

static const float timeToTide = 40.0f;
static const float tideEventDisplayTime = 5.0f;
static const float wySeaStart = float(syBeachMax + syCrenelHeight) / float(syScreenHeight);
static const float wdySeaRiseRate = 1.0f / 300.0f;

static const float windWoodPushRate = 1.0f/0.3f;

// particle fall rate is inversely proportional to size of particle
static const float particleMoveRate = 40.0f * syCellHeight / 16;

enum ActionState {
	Idle,
	GettingSand,
	GettingWater,
	GettingDampSand,
	GettingWood,
	PouringSand,
	PouringWater,
	PouringDampSand,
	SettingLadder
};

enum Particle {
	NoParticle,
	DrySand,
	DampSand
};

void swap(Particle grid[nyParticles][nxParticles], int x1, int y1, int x2, int y2) {
	Particle t1 = grid[y1][x1];
	Particle t2 = grid[y2][x2];
	grid[y2][x2] = t1;
	grid[y1][x1] = t2;
}

bool swapIfEmpty(Particle grid[nyParticles][nxParticles], int x1, int y1, int x2, int y2) {
	if (x2 >= 0 && x2 < nxParticles && y2 >= 0 && y2 < nyParticles && grid[y2][x2] == NoParticle) {
		swap(grid, x1, y1, x2, y2);
		return true;
	}
	else {
		return false;
	}
}

enum CastleCell {
	NonFullCell,
	FullDryCell,
	FullDampCell
};

enum BucketState {
	BucketEmpty,
	BucketSand,
	BucketWater,
	BucketDampSand,
	BucketWood
};

enum GameState {
	Normal,
	Drowning,
	Menu,
	Won
};

class Game : public olc::PixelGameEngine
{
public:
	Game()
	{
		sAppName = "Beach Weather";
	}

private:

	float sxPlayerX = 60.0f;
	float syPlayerY = float(syBeachMax - 1);

	bool raining = false;
	float rainCharge = 0.0f;
	std::vector<float> wxRaindropsX;
	std::vector<float> wyRaindropsY;

	bool wind;
	float windVelocity;

	bool seaRising;
	float wySeaLevel;

	ActionState actionState = Idle;

	Particle particles[nyParticles][nxParticles];
	CastleCell castleGrid[nyCells][nxCells];

	std::vector<olc::vi2d> ladders;
	std::vector<olc::vi2d> looseLadders;
	bool nearUpLadder;
	bool nearDownLadder;

	std::vector<olc::vi2d> sunburnLocations;
	std::vector<float> sunburnTimes;

	int cxPlayerX;
	int cyPlayerY;

	bool debug = false;

	void drawCrenel(int sx, int sy) {
		// fill one extra line dark yellow height to overwrite "lid" of the block
		FillRect(sx + 1, sy - syCrenelHeight, sxCrenelWidth - 2, syCrenelHeight + 1, olc::DARK_YELLOW);
		DrawLine(sx, sy - syCrenelHeight, sx, sy - 1, olc::VERY_DARK_YELLOW);
		DrawLine(sx + sxCrenelWidth - 1, sy - syCrenelHeight, sx + sxCrenelWidth - 1, sy - 1, olc::VERY_DARK_YELLOW);
		DrawLine(sx, sy - syCrenelHeight, sx + sxCrenelWidth - 1, sy - syCrenelHeight, olc::VERY_DARK_YELLOW);
	}

	void updateCellFromParticles(int cx, int cy) {
		int sTop = cy * syCellHeight;
		int sLeft = cx * sxCellWidth;
		Particle firstParticle = particles[sTop][sLeft];
		if (firstParticle == NoParticle) {
			castleGrid[cy][cx] = NonFullCell;
		}
		else {
			Particle cellType = firstParticle;
			for (int x = 0; x < sxCellWidth; x++) {
				for (int y = 0; y < syCellHeight; y++) {
					cellType = std::min(cellType, particles[sTop + y][sLeft + x]);
				}
			}
			switch (cellType) {
			case NoParticle:
				castleGrid[cy][cx] = NonFullCell;
				break;
			case DrySand:
				castleGrid[cy][cx] = FullDryCell;
				break;
			case DampSand:
				castleGrid[cy][cx] = FullDampCell;
				break;
			}
		}
	}


	BucketState bucket;

	float sunburnEventCharge;
	float windEventCharge;
	float windEventStopCharge;
	float particleMoveCharge;
	float rainTimer;

	bool tideCounting;
	float tideTimer;
	bool displayingTideEvent;
	float tideEventDisplayTimer;

	float windWoodPushCharge;

	int fallPreference;

	GameState gameState = Menu;

	void resetGameVariables() {
		sxPlayerX = 60.0f;
		syPlayerY = float(syBeachMax - 1);

		raining = false;
		rainCharge = 0.0f;

		wind = false;
		windVelocity = windSpeed;

		seaRising = false;
		wySeaLevel = wySeaStart;
		displayingTideEvent = false;
		tideEventDisplayTimer = 0.0f;

		windWoodPushCharge = 0.0f;

		actionState = Idle;

		for (int y = 0; y < nyParticles; y++) {
			for (int x = 0; x < nxParticles; x++) {
				particles[y][x] = NoParticle;
			}
		}
		for (int y = 0; y < nyCells; y++) {
			for (int x = 0; x < nxCells; x++) {
				castleGrid[y][x] = NonFullCell;
			}
		}

		ladders.clear();
		looseLadders.clear();
		nearUpLadder = false;
		nearDownLadder = false;

		sunburnLocations.clear();
		sunburnTimes.clear();

		tideCounting = true;
		tideTimer = 0.0f;

		rainTimer = 0.0f;

		debug = false;

		bucket = BucketEmpty;

		sunburnEventCharge = 0.0f;
		windEventCharge = 0.0f;
		windEventStopCharge = 0.0f;
		particleMoveCharge = 0.0f;
	}

	const int sxBucketWidth = 3 * sxCrenelOffset + sxCrenelWidth;
	const int syBucketHeight = syScreenHeight - syBeachMax - 1;

	void FillBucket(olc::Pixel pixel) {
		FillRect(sxScreenWidth - sxBucketWidth + 1, syBeachMax + 1, sxBucketWidth - 2, syBucketHeight - 1, pixel);
	}

	void FillKey(int x, int y, olc::Pixel pixel, std::string letter, bool valid) {
		DrawRect(x, y, sxKeyWidth, syKeyHeight, olc::GREY);
		if (valid) {
			FillRect(x + 1, y + 1, sxKeyWidth - 1, syKeyHeight - 1, pixel);
			DrawString(x + (sxKeyWidth - 1) / 2 - letterSize / 2 + 1, y + (syKeyHeight - 1) / 2 - letterSize / 2 + 1, letter);
		}
		else {
			DrawString(x + (sxKeyWidth - 1) / 2 - letterSize / 2 + 1, y + (syKeyHeight - 1) / 2 - letterSize / 2 + 1, letter, olc::GREY);
		}
	}

	void writeCentred(int x, int y, std::string s) {
		int left = x - letterSize*s.length()/2 - 1;
		int up = y - letterSize / 2 - 1;
		DrawString(left, up, s);
	}

	void dropLadder() {
		looseLadders.push_back(olc::vi2d(
			sxPlayerX + sxPlayerWidth / 2 - sxCellsOffset / 2,
			syPlayerY - syCellHeight / 4 + 1
		));
	}

public:
	bool OnUserCreate() override
	{
		return true;
	}

	bool OnUserUpdate(float fElapsedTime) override
	{
		if (gameState == Menu) {
			Clear(olc::CYAN);
			// draw beach
			FillRect(0, syBeachMax, sxScreenWidth, syScreenHeight - syBeachMax, olc::DARK_YELLOW);
			DrawLine(0, syBeachMax, sxScreenWidth - 1, syBeachMax, olc::VERY_DARK_YELLOW);
			DrawString(sxScreenWidth/2 - 40 - 1, syScreenHeight/2 - 4 - 1, "F to start");
			if (GetKey(olc::Key::F).bPressed) {
				gameState = Normal;
				resetGameVariables();
			}
			return(true);
		}

		Clear(olc::CYAN);

		if (seaRising && wySeaLevel > 0) wySeaLevel -= wdySeaRiseRate * fElapsedTime;
		int sySeaLevel = int(wySeaLevel * syScreenHeight);

		// sunburn event timer
		sunburnEventCharge += fElapsedTime * sunburnEventRate;
		if (sunburnEventCharge >= 1.0f) {
			std::vector<olc::vi2d> burnable;
			for (int x = 0; x < nxCells; x++) {
				for (int y = 0; y < nyCells; y++) {
					if (castleGrid[y][x] == FullDampCell && syCellHeight * (y + 1) - 1 < sySeaLevel) {
						burnable.push_back(olc::vi2d(x, y));
					}
				}
			}
			// apply sunburn to five damp cells
			int n_burnable = burnable.size();
			int to_burn = 5;
			for (auto& pos : burnable) {
				float prob = std::min(float(to_burn) / float(n_burnable), 1.0f);
				if (float(std::rand()) / float(RAND_MAX) < prob) {
					bool inBurns = std::find(
						sunburnLocations.begin(),
						sunburnLocations.end(),
						olc::vi2d(pos.x, pos.y)
					) != sunburnLocations.end();
					if (!inBurns) {
						sunburnLocations.push_back(olc::vi2d(pos.x, pos.y));
						sunburnTimes.push_back(sunburnTime);
						to_burn -= 1;
					}
				}
				n_burnable -= 1;
				if (to_burn == 0) {
					break;
				}
			}

			while (sunburnEventCharge >= 1.0f) {
				sunburnEventCharge -= 1.0f;
			}
		}

		// wind event stop timer
		if (wind) {
			windEventStopCharge += fElapsedTime;
			if (windEventStopCharge >= windEventDuration) {
				wind = false;
				windEventStopCharge = 0.0f;
			}
		}

		// wind event timer
		windEventCharge += fElapsedTime * windEventRate;
		if (windEventCharge >= 1.0f) {
			wind = true;
			windVelocity = windVelocity * float(1 - 2 * (std::rand() % 2));
			windEventCharge -= 1.0f;
		}

		// rain event timer
		if (!raining) {
			rainTimer += fElapsedTime;
			if (rainTimer >= timeToRain) {
				raining = true;
			}
		}

		// tide event timer
		if (tideCounting) {
			tideTimer += fElapsedTime;
			if (tideTimer >= timeToTide) {
				tideCounting = false;
				seaRising = true;
				displayingTideEvent = true;
			}
		}

		// tide event display timer
		if (displayingTideEvent) {
			tideEventDisplayTimer += fElapsedTime;
			if (tideEventDisplayTimer >= tideEventDisplayTime) {
				displayingTideEvent = false;
			}
		}

		// sunburn timer
		if (gameState != Won) {
			for (int n = 0; n < sunburnLocations.size();) {
				sunburnTimes[n] -= fElapsedTime;
				if (sunburnTimes[n] <= 0.0f) {
					olc::vi2d cell = sunburnLocations[n];

					castleGrid[cell.y][cell.x] = NonFullCell; // dry out cell here, removing now for placeholder
					for (int x = cell.x * sxCellWidth; x < (cell.x + 1) * sxCellWidth; x++) {
						for (int y = cell.y * syCellHeight; y < (cell.y + 1) * syCellHeight; y++) {
							particles[y][x] = DrySand;
						}
					}

					sunburnLocations.erase(sunburnLocations.begin() + n);
					sunburnTimes.erase(sunburnTimes.begin() + n);
				}
				else {
					n++;
				}
			}
		}

		// tide raises loose ladders
		for (auto& pos : looseLadders) {
			if (pos.y + syCellHeight/4 > sySeaLevel) {
				pos.y = sySeaLevel - syCellHeight/4;
			}
		}

		// tide wets sand
		int syTideWetHeight = sySeaLevel - syCrenelHeight;
		for (int x = 0; x < nxParticles; x++) {
			for (int y = syTideWetHeight; y < nyParticles; y++) {
				if (particles[y][x] == DrySand) {
					particles[y][x] = DampSand;
				}
			}
		}

		// wind pushes floating ladders
		if (wind) {
			windWoodPushCharge += windWoodPushRate * fElapsedTime;
			while (windWoodPushCharge >= 1.0f) {
				for (olc::vi2d& pos : looseLadders) {
					if (pos.y == sySeaLevel - syCellHeight / 4) {
						if (windVelocity > 0.0f && pos.x + sxCellsOffset - 1 < sxScreenWidth - 1) {
							pos.x += 1;
						}
						else {
							if (windVelocity < 0.0f && pos.x > sxCellsOffset) {
								pos.x -= 1;
							}
						}
					}
				}
				windWoodPushCharge -= 1.0f;
			}
		}

		// particles fall
		// if wind blowing, dry sand can move to the downwind side
		if (gameState != Won) {
			particleMoveCharge += fElapsedTime * particleMoveRate;
		}
		int updateDirection;
		while (particleMoveCharge >= 1.0f) {
			for (int y = nyParticles - 1; y >= 0; y--) {
				if (!wind) {
					updateDirection = std::rand() % 2;
				}
				else {
					updateDirection = int(windVelocity < 0.0f);
				}
				for (int x = (nxParticles - 1) * updateDirection; x >= 0 && x < nxParticles; x += 1 - 2*updateDirection) {
					bool leftDamp = x > 0 && particles[y][x - 1] == DampSand;
					bool rightDamp = x < nxParticles - 1 && particles[y][x + 1] == DampSand;
					if (wind) {
						fallPreference = 2*int(windVelocity > 0.0f) - 1;
					}
					else {
						fallPreference = 2*(std::rand() % 2) - 1;
					}
					switch(particles[y][x]) {
					case DrySand:
						if (!wind) {
							if (!swapIfEmpty(particles, x, y, x, y + 1)) {
								if (!swapIfEmpty(particles, x, y, x + fallPreference, y + 1)) {
									swapIfEmpty(particles, x, y, x - fallPreference, y + 1);
								}
							}
						}
						else {
							if (!swapIfEmpty(particles, x, y, x + fallPreference, y + 1)) {
								if (!swapIfEmpty(particles, x, y, x, y + 1)) {
									if (!swapIfEmpty(particles, x, y, x + fallPreference, y)) {
										swapIfEmpty(particles, x, y, x - fallPreference, y + 1);
									}
								}
							}
						}
						break;
					case DampSand:
						if (y < nyParticles - 1) {
							if (!swapIfEmpty(particles, x, y, x, y + 1)) {
								if (!leftDamp && !rightDamp) {
									if (!swapIfEmpty(particles, x, y, x + fallPreference, y + 1)) {
										swapIfEmpty(particles, x, y, x - fallPreference, y + 1);
									}
								}
							}
						}
						break;
					case NoParticle:
						break;
					}
				}
			}
			particleMoveCharge -= 1.0f;
		}

		// update tiles from particles
		for (int cx = 0; cx < nxCells; cx++) {
			for (int cy = 0; cy < nyCells; cy++) {
				updateCellFromParticles(cx, cy);
			}
		}

		// make ladders loose if cells now empty or underwater
		for (int l = 0; l < ladders.size(); l++) {
			olc::vi2d pos = ladders[l];
			bool underWater = sySeaLevel <= pos.y*syCellHeight;
			if (castleGrid[pos.y][pos.x] == NonFullCell || underWater) {
				looseLadders.push_back(olc::vi2d(sxCellsOffset + pos.x * sxCellWidth, (pos.y + 1)*syCellHeight - syCellHeight/4));
				ladders.erase(ladders.begin() + l);
			}
		}

		bool inBounds;
		bool nearTree;
		int nearLooseLadder;
		bool nearWater;
		bool canGetSand;
		bool canGetWater;
		bool canGetWood;
		bool canDump;
		bool falling;

		switch (gameState) {
		case Won:
			if (GetKey(olc::Key::F).bPressed) {
				gameState = Menu;
			}
			canGetSand = false;
			canGetWater = false;
			canGetWood = false;
			canDump = false;
			falling = false;
			break;
		case Drowning:
			if (GetKey(olc::Key::F).bPressed) {
				gameState = Menu;
			}
			if (syPlayerY - syPlayerHeight + 1 < syScreenHeight) {
				syPlayerY += syPlayerDrownSpeed * fElapsedTime;
			}
			// final player cell
			cxPlayerX = floor((sxPlayerX + sxPlayerWidth / 2 - sxCellsOffset) / sxCellWidth);
			cyPlayerY = floor(syPlayerY / syCellHeight);
			canGetSand = false;
			canGetWater = false;
			canGetWood = false;
			canDump = false;
			falling = false;
			break;
		case Normal:
			if (GetKey(olc::Key::LEFT).bHeld) sxPlayerX -= sxPlayerSpeed * fElapsedTime;
			if (GetKey(olc::Key::RIGHT).bHeld) sxPlayerX += sxPlayerSpeed * fElapsedTime;
			if (GetKey(olc::Key::UP).bHeld && nearUpLadder) syPlayerY -= syPlayerSpeed * fElapsedTime;
			if (GetKey(olc::Key::DOWN).bHeld && nearDownLadder) syPlayerY += syPlayerSpeed * fElapsedTime;

			// falling
			int cxPlayerLeftX = floor((sxPlayerX - sxCellsOffset) / sxCellWidth);
			int cxPlayerRightX = floor((sxPlayerX + sxPlayerWidth - 1 - sxCellsOffset) / sxCellWidth);
			int cyInterPlayerY = floor(syPlayerY / syCellHeight);
			bool leftCanStand = castleGrid[cyInterPlayerY + 1][cxPlayerLeftX] == FullDampCell;
			bool rightCanStand = castleGrid[cyInterPlayerY + 1][cxPlayerRightX] == FullDampCell;
			bool onWorldFloor = cyInterPlayerY == nyCells - 1;
			bool canStand = onWorldFloor || leftCanStand || rightCanStand;
			bool onCellFloor = (int(syPlayerY) % syCellHeight) == syCellHeight - 1;
			bool wouldFall = !canStand || !onCellFloor;
			if (wouldFall && !nearUpLadder && !nearDownLadder) {
				syPlayerY += syPlayerFallSpeed * fElapsedTime;
			}

			// final player position / cell
			sxPlayerX = std::max(std::min(sxPlayerX, float(sxScreenWidth - sxPlayerWidth)), 0.0f - float(sxPlayerWidth) / 2.0f);
			syPlayerY = std::max(std::min(syPlayerY, float(syBeachMax - 1)), float(syPlayerHeight - 1));
			cxPlayerX = floor((sxPlayerX + sxPlayerWidth / 2 - sxCellsOffset) / sxCellWidth);
			cyPlayerY = floor(syPlayerY / syCellHeight);

			// check for win/lose states
			if (syPlayerY - syPlayerHeight + 1 >= sySeaLevel) {
				gameState = Drowning;
			}
			if (syPlayerY < syCellHeight) {
				gameState = Won;
			}

			inBounds = cxPlayerX >= 0 && cxPlayerX < nxCells&& cyPlayerY >= 0 && cyPlayerY < nyCells;
			nearTree = (cxPlayerX == -1 && cyPlayerY == nyCells - 1);
			nearLooseLadder = -1;
			for (int n = 0; n < looseLadders.size(); n++) {
				olc::vi2d pos = looseLadders[n];
				if (
					pos.x <= sxPlayerX + sxPlayerWidth - 1
					&& pos.x + sxCellsOffset - 1 >= sxPlayerX
					&& syPlayerY >= pos.y - syCrenelHeight
					&& syPlayerY - syPlayerHeight + 1 <= pos.y + syCellHeight/4 - 1
				) {
					nearLooseLadder = n;
					break;
				}
			}
			// water reachable from one less than sea block-wetting distance, so starts possible
			nearWater = (syPlayerY >= sySeaLevel - syCrenelHeight - 1);
			bool ladderInCell = false;
			bool ladderInBelowCell = false;
			for (int l = 0; l < ladders.size(); l++) {
				olc::vi2d ladder = ladders[l];
				if (ladder.x == cxPlayerX && ladder.y == cyPlayerY) {
					ladderInCell = true;
					if (ladderInBelowCell) break;
				}
				if (ladder.x == cxPlayerX && ladder.y == cyPlayerY + 1) {
					ladderInBelowCell = true;
					if (ladderInCell) break;
				}
			};
			nearUpLadder = ladderInCell;
			nearDownLadder = (
				ladderInCell && int(syPlayerY) % syCellHeight != syCellHeight - 1)
				|| (ladderInBelowCell && onCellFloor
			);
			falling = wouldFall && !nearUpLadder && !nearDownLadder;
			actionState = Idle;
			canGetSand = !falling;
			canGetWater = !falling && nearWater;
			canGetWood = !falling && (nearTree || nearLooseLadder >= 0);
			canDump = inBounds && bucket != BucketEmpty;
			if (GetKey(olc::Key::S).bPressed && canGetSand) {
				if (bucket == BucketWater) {
					actionState = GettingDampSand;
				}
				else {
					actionState = GettingSand;
				}
			}
			if (GetKey(olc::Key::A).bPressed && canGetWater) {
				if (bucket == BucketSand) {
					actionState = GettingDampSand;
				}
				else {
					actionState = GettingWater;
				}
			}
			if (GetKey(olc::Key::W).bPressed && canGetWood) actionState = GettingWood;
			if (GetKey(olc::Key::D).bPressed && canDump) {
				switch (bucket) {
				case BucketSand:
					actionState = PouringSand;
					break;
				case BucketWater:
					actionState = PouringWater;
					break;
				case BucketDampSand:
					actionState = PouringDampSand;
					break;
				case BucketWood:
					actionState = SettingLadder;
					break;
				}
			}

			if (GetKey(olc::Key::R).bPressed) raining = !raining;
			if (GetKey(olc::Key::B).bPressed) {
				wind = !wind;
				windVelocity = windVelocity * float(1 - 2 * (std::rand() % 2));
			}
			if (GetKey(olc::Key::T).bPressed) seaRising = !seaRising;
			if (GetKey(olc::Key::G).bPressed) debug = !debug;

			switch (actionState) {
			case GettingSand:
				if (bucket == BucketWood) {
					dropLadder();
				}
				bucket = BucketSand;
				break;
			case GettingWater:
				if (bucket == BucketWood) {
					dropLadder();
				}
				bucket = BucketWater;
				break;
			case GettingDampSand:
				if (bucket == BucketWood) {
					dropLadder();
				}
				bucket = BucketDampSand;
				break;
			case GettingWood:
				if (bucket == BucketWood) {
					dropLadder();
				}
				bucket = BucketWood;
				if (!nearTree) {
					looseLadders.erase(looseLadders.begin() + nearLooseLadder);
				}
				break;
			case PouringSand:
				switch (castleGrid[cyPlayerY][cxPlayerX]) {
				case NonFullCell:
					castleGrid[cyPlayerY][cxPlayerX] = FullDryCell;
					for (int x = cxPlayerX * sxCellWidth; x < (cxPlayerX + 1) * sxCellWidth; x++) {
						for (int y = cyPlayerY * syCellHeight; y < (cyPlayerY + 1) * syCellHeight; y++) {
							if (particles[y][x] != DampSand) {
								particles[y][x] = DrySand;
							}
						}
					}
					bucket = BucketEmpty;
					break;
				}
				break;
			case PouringWater:
				switch (castleGrid[cyPlayerY][cxPlayerX]) {
				case FullDryCell:
					castleGrid[cyPlayerY][cxPlayerX] = FullDampCell;
					for (int x = cxPlayerX * sxCellWidth; x < (cxPlayerX + 1) * sxCellWidth; x++) {
						for (int y = cyPlayerY * syCellHeight; y < (cyPlayerY + 1) * syCellHeight; y++) {
							particles[y][x] = DampSand;
						}
					}
					bucket = BucketEmpty;
				case FullDampCell:
					for (int b = 0; b < sunburnLocations.size(); b++) {
						if (sunburnLocations[b] == olc::vi2d(cxPlayerX, cyPlayerY)) {
							sunburnLocations.erase(sunburnLocations.begin() + b);
							sunburnTimes.erase(sunburnTimes.begin() + b);
							bucket = BucketEmpty;
							break;
						}
					}
				}
				break;
			case PouringDampSand:
				switch (castleGrid[cyPlayerY][cxPlayerX]) {
				case NonFullCell:
				case FullDryCell:
					castleGrid[cyPlayerY][cxPlayerX] = FullDampCell;
					for (int x = cxPlayerX * sxCellWidth; x < (cxPlayerX + 1) * sxCellWidth; x++) {
						for (int y = cyPlayerY * syCellHeight; y < (cyPlayerY + 1) * syCellHeight; y++) {
							particles[y][x] = DampSand;
						}
					}
					bucket = BucketEmpty;
					break;
				}
				break;
			case SettingLadder:
				bool hasLadder = false;
				switch (castleGrid[cyPlayerY][cxPlayerX]) {
				case FullDryCell:
				case FullDampCell:
					for (olc::vi2d& pos : ladders) {
						if (pos.x == cxPlayerX && pos.y == cyPlayerY) {
							hasLadder = true;
							break;
						}
					}
					if (hasLadder) {
						dropLadder();
					}
					else {
						ladders.push_back({ cxPlayerX, cyPlayerY });
					}
					bucket = BucketEmpty;
					break;
				case NonFullCell:
					dropLadder();
					bucket = BucketEmpty;
					break;
				}
				break;
			}

			break;
		}

		// draw sun, hotter if any blocks burning
		if (sunburnLocations.size() > 0) {
			FillCircle(sxScreenWidth - sSunRadius - 1, sSunRadius, sSunRadius, olc::RED);
		}
		else {
			FillCircle(sxScreenWidth - sSunRadius - 1, sSunRadius, sSunRadius, olc::YELLOW);
		}

		// draw THE MOON
		// tidal force proportional to 1/distance^2
		// so distance prop 1/height^(1/2),
		// size prop 1/distance prop height^(1/2);
		float seaProgress = 1.0f - wySeaLevel / wySeaStart;
		FillCircle(
			sxMoonStart + (sxMoonTarget - sxMoonStart)*std::pow(seaProgress, 3.0f),
			syMoonStart + (syMoonTarget - syMoonStart)*std::pow(seaProgress, 3.0f),
			sxScreenWidth*seaProgress,
			olc::WHITE
		);

		// draw beach
		FillRect(0, syBeachMax, sxScreenWidth, syScreenHeight - syBeachMax, olc::DARK_YELLOW);
		DrawLine(0, syBeachMax, sxScreenWidth - 1, syBeachMax, olc::VERY_DARK_YELLOW);

		// draw debug grid lines
		if (debug) {
			for (int cx = 0; cx < nxCells; cx++) {
				int sx = cx * sxCellWidth + sxCellsOffset;
				DrawLine(sx, 0, sx, syBeachMax, olc::GREY);
			}
			for (int cy = 0; cy <= nyCells; cy++) {
				int sy = cy * syCellHeight;
				DrawLine(sxCellsOffset, sy, sxScreenWidth - 1, sy, olc::GREY);
			}
		}

		// draw particles
		for (int sx = 0; sx < nxParticles; sx++) {
			for (int sy = 0; sy < nyParticles; sy++) {
				switch (particles[sy][sx]) {
				case DrySand:
					Draw(sxCellsOffset + sx, sy, olc::YELLOW);
					break;
				case DampSand:
					Draw(sxCellsOffset + sx, sy, olc::DARK_YELLOW);
					break;
				}
			}
		}

		// draw castle area
		for (int y = 0; y < nyCells; y++) {
			for (int x = 0; x < nxCells; x++) {
				if (castleGrid[y][x] == FullDampCell) {
					DrawLine(sxCellsOffset + sxCellWidth * x, syCellHeight * y, sxCellsOffset + sxCellWidth * (x + 1) - 1, syCellHeight * y, olc::VERY_DARK_YELLOW);
				}
				if (castleGrid[y][x] == FullDryCell) {
					DrawLine(sxCellsOffset + sxCellWidth * x, syCellHeight * y, sxCellsOffset + sxCellWidth * (x + 1) - 1, syCellHeight * y, olc::DARK_YELLOW);
				}
			}
		}

		// redraw cells drying out
		for (int n = 0; n < sunburnLocations.size(); n++) {
			olc::vi2d pos = sunburnLocations[n];
			float time = sunburnTimes[n];
			olc::Pixel col = olc::PixelLerp(olc::YELLOW, olc::DARK_YELLOW, time / sunburnTime);
			FillRect(sxCellsOffset + sxCellWidth * pos.x, syCellHeight* pos.y, sxCellWidth, syCellHeight, col);
			DrawLine(sxCellsOffset + sxCellWidth * pos.x, syCellHeight* pos.y, sxCellsOffset + sxCellWidth * (pos.x + 1) - 1, syCellHeight* pos.y, olc::VERY_DARK_YELLOW);
		}
		// draw ladders
		for (int l = 0; l < ladders.size(); l++) {
			olc::vi2d pos = ladders[l];
			int x = pos.x;
			int y = pos.y;
			DrawLine(sxCellsOffset + sxCellWidth * x + sxLadderOffset, syCellHeight * y + 1, sxCellsOffset + sxCellWidth * x + sxLadderOffset, syCellHeight* (y + 1) - 1, brown);
			DrawLine(sxCellsOffset + sxCellWidth * (x + 1) - sxLadderOffset - 1, syCellHeight* y + 1, sxCellsOffset + sxCellWidth * (x + 1) - sxLadderOffset - 1, syCellHeight* (y + 1) - 1, brown);
			DrawLine(sxCellsOffset + sxCellWidth * x + sxLadderOffset, syCellHeight*y + syCrenelHeight, sxCellsOffset + sxCellWidth * (x + 1) - sxLadderOffset - 1, syCellHeight* y + syCrenelHeight, brown);
			DrawLine(sxCellsOffset + sxCellWidth * x + sxLadderOffset, syCellHeight* y + 2 * syCrenelHeight, sxCellsOffset + sxCellWidth * (x + 1) - sxLadderOffset - 1, syCellHeight* y + 2 * syCrenelHeight, brown);
			DrawLine(sxCellsOffset + sxCellWidth * x + sxLadderOffset, syCellHeight*y + 3*syCrenelHeight, sxCellsOffset + sxCellWidth * (x + 1) - sxLadderOffset - 1, syCellHeight*y + 3*syCrenelHeight, brown);
		}

		// draw fire effect on cells drying out
		for (int n = 0; n < sunburnLocations.size(); n++) {
			olc::vi2d pos = sunburnLocations[n];
			FillRect(sxCellsOffset + sxCellWidth * pos.x, syCellHeight * pos.y + 3*syCrenelHeight, sxCellWidth, syCrenelHeight, olc::RED);
		}

		// draw cliffs
		FillRect(0, syCellHeight - syCellHeight/4, sxCellsOffset/2, syBeachMax - syCellHeight, olc::GREY);
		FillRect(0, syCellHeight - syCellHeight / 4, sxCellsOffset/2, sxCrenelOffset, olc::GREEN);

		// draw wood pile
		FillRect(0, syBeachMax - syCellHeight/4, sxCellsOffset, syCellHeight/4, brown);
		DrawRect(0, syBeachMax - syCellHeight / 4, sxCellsOffset - 1, syCellHeight / 4 - 1, olc::BLACK);
		FillRect(0, syBeachMax - syCellHeight / 4 - syCellHeight/4, sxCellsOffset, syCellHeight / 4, brown);
		DrawRect(0, syBeachMax - syCellHeight / 4 - syCellHeight / 4, sxCellsOffset - 1, syCellHeight / 4 - 1, olc::BLACK);

		// draw loose ladders
		for (auto& pos : looseLadders) {
			FillRect(pos.x, pos.y, sxCellsOffset, syCellHeight / 4, brown);
			DrawRect(pos.x, pos.y, sxCellsOffset - 1, syCellHeight / 4 - 1, olc::BLACK);
		}

		// draw crenellations above player
		for (int y = 0; y < cyPlayerY + 1; y++) {
			for (int x = 0; x < nxCells; x++) {
				if (castleGrid[y][x] == FullDampCell) {
					drawCrenel(sxCellsOffset + sxCrenelOffset + sxCellWidth * x, syCellHeight * y);
					drawCrenel(sxCellsOffset + 3 * sxCrenelOffset + sxCrenelWidth + sxCellWidth * x, syCellHeight * y);
				}
			}
		}

		// draw player
		FillRect(sxPlayerX, syPlayerY - syPlayerHeight + 1, sxPlayerWidth, syPlayerHeight, olc::MAGENTA);

		// draw crenellations below player
		for (int y = cyPlayerY + 1; y < nyCells; y++) {
			for (int x = 0; x < nxCells; x++) {
				if (castleGrid[y][x] == FullDampCell) {
					drawCrenel(sxCellsOffset + sxCrenelOffset + sxCellWidth * x, syCellHeight * y);
					drawCrenel(sxCellsOffset + 3 * sxCrenelOffset + sxCrenelWidth + sxCellWidth * x, syCellHeight * y);
				}
			}
		}

		// draw sea
		SetPixelBlend(0.7f);
		SetPixelMode(olc::Pixel::ALPHA);
		FillRect(0, sySeaLevel, sxScreenWidth, syScreenHeight - sySeaLevel, olc::BLUE);
		SetPixelMode(olc::Pixel::NORMAL);

		// rainfall
		if (wind) {
			for (auto& x : wxRaindropsX) {
				x += windVelocity * fElapsedTime;
			}
		}
		for (auto& y : wyRaindropsY) {
			y += rainFallSpeed*fElapsedTime;
		}
		for (int n = 0; n < wxRaindropsX.size(); n++) {
			if (wyRaindropsY[n] > wySeaLevel || wxRaindropsX[n] < wxMinRainX || wxRaindropsX[n] > wxMaxRainX) {
				wxRaindropsX.erase(wxRaindropsX.begin() + n);
				wyRaindropsY.erase(wyRaindropsY.begin() + n);
			}
		}
		if (raining) {
			rainCharge += fElapsedTime*rainRate;
			while (rainCharge >= 1.0f) {
				float wxNewX = wxMinRainX + (wxMaxRainX - wxMinRainX)*std::rand() / float(RAND_MAX);
				int sxNewX = floor(sxScreenWidth * wxNewX);
				wxRaindropsX.push_back(wxNewX);
				wyRaindropsY.push_back(0.0);
				rainCharge -= 1.0f;
			}
		}
		for (int n = 0; n < wxRaindropsX.size(); n++) {
			float wx = wxRaindropsX[n];
			float wy = wyRaindropsY[n];
			FillRect(wx*sxScreenWidth, wy*syScreenHeight, 4/pixels, 4/pixels, olc::BLUE);
		}

		// draw controls UI
		switch (bucket) {
		case BucketEmpty:
			FillKey(0, syScreenHeight - syKeyHeight - 1, olc::BLUE, "A", canGetWater);
			FillKey(sxKeyWidth + sKeySpacing, syScreenHeight - syKeyHeight - 1, olc::YELLOW, "S", canGetSand);
			break;
		case BucketSand:
			FillKey(0, syScreenHeight - syKeyHeight - 1, olc::DARK_YELLOW, "A", canGetWater);
			FillKey(sxKeyWidth + sKeySpacing, syScreenHeight - syKeyHeight - 1, olc::YELLOW, "S", canGetSand);
			break;
		case BucketDampSand:
			FillKey(0, syScreenHeight - syKeyHeight - 1, olc::BLUE, "A", canGetWater);
			FillKey(sxKeyWidth + sKeySpacing, syScreenHeight - syKeyHeight - 1, olc::YELLOW, "S", canGetSand);
			break;
		case BucketWater:
			FillKey(0, syScreenHeight - syKeyHeight - 1, olc::BLUE, "A", canGetWater);
			FillKey(sxKeyWidth + sKeySpacing, syScreenHeight - syKeyHeight - 1, olc::DARK_YELLOW, "S", canGetSand);
			break;
		case BucketWood:
			FillKey(0, syScreenHeight - syKeyHeight - 1, olc::BLUE, "A", canGetWater);
			FillKey(sxKeyWidth + sKeySpacing, syScreenHeight - syKeyHeight - 1, olc::YELLOW, "S", canGetSand);
			break;
		}
		FillKey(sxKeyWidth + sKeySpacing, syScreenHeight - 2 * syKeyHeight - sKeySpacing - 1, brown, "W", canGetWood);
		FillKey(2 * (sxKeyWidth + sKeySpacing), syScreenHeight - syKeyHeight - 1, olc::CYAN, "D", canDump);

		// draw bucket UI
		FillRect(sxScreenWidth - sxBucketWidth, syBeachMax + 1, sxBucketWidth, syBucketHeight, olc::GREY);
		switch(bucket) {
		case BucketEmpty:
			FillBucket(olc::CYAN);
			break;
		case BucketSand:
			FillBucket(olc::YELLOW);
			break;
		case BucketWater:
			FillBucket(olc::DARK_BLUE);
			break;
		case BucketDampSand:
			FillBucket(olc::DARK_YELLOW);
			break;
		case BucketWood:
			FillBucket(brown);
			break;
		}

		// status messages
		if (gameState == Won) {
			writeCentred(sxScreenWidth / 2, syScreenHeight / 2, "You made it!");
			writeCentred(sxScreenWidth / 2, syScreenHeight / 2 + letterSize, "Press F for menu");
		}
		if (gameState == Drowning) {
			writeCentred(sxScreenWidth / 2, syScreenHeight / 2, "Oops...");
			writeCentred(sxScreenWidth / 2, syScreenHeight / 2 + letterSize, "Press F for menu");
		}
		if (gameState == Normal && displayingTideEvent) {
			writeCentred(sxScreenWidth / 2, syScreenHeight / 2, "The tide is coming in!");
		}

		DrawString(0, 0, std::to_string(nearDownLadder));

		return true;
	}

};


int main()
{
	Game demo;
	if (demo.Construct(sxScreenWidth, syScreenHeight, pixels, pixels))
		demo.Start();

	return 0;
}