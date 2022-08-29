#define OLC_PGE_APPLICATION
#include "olcPixelGameEngine.h"

static const int sxCrenelOffset = 2;

static const int sxCrenelWidth = 2 * sxCrenelOffset; // _cc__cc_
static const int syCrenelHeight = sxCrenelWidth; // square crenels

static const int syHalfcellHeight = 2 * syCrenelHeight;

static const int sxCellWidth = 4 * sxCrenelOffset + 2 * sxCrenelWidth; // _cc__cc_
static const int syCellHeight = 4 * syCrenelHeight; // must be multiple of four for crenellations

static const int nxCells = 9;
static const int nyCells = 9;

static const int sxCellsOffset = sxCellWidth; // non-build area on left edge for trees, cliffs etc.

static const int sxScreenWidth = sxCellsOffset + sxCellWidth * nxCells;
static const int syBeachMax = syCellHeight * nyCells;
static const int syBottom = 12;
static const int syScreenHeight = syBeachMax + syCellHeight;

static const int sxPlayerWidth = 3 * sxCrenelOffset;
static const int syPlayerHeight = 6 * sxCrenelOffset;
static const float sxPlayerSpeed = float(2 * sxCellWidth);
static const float syPlayerSpeed = float(2 * syCellHeight);
static const float syPlayerFallSpeed = float(4 * syCellHeight);

static const int sxLadderOffset = sxCrenelOffset + sxCrenelWidth - 1;

static const olc::Pixel brown = olc::Pixel(128, 0, 64);

static const float sunburnTime = 5.0f;

enum ActionState {
	Idle,
	GettingSand,
	GettingWater,
	GettingWood,
	PouringSand,
	PouringWater,
	SettingLadder
};

enum CastleCell {
	Empty,
	HalfDry,
	HalfDamp,
	TopDry,
	Complete
};

enum BucketState {
	BucketEmpty,
	BucketSand,
	BucketWater,
	BucketWood
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
	const float rainRate = 50.0f;
	const float rainFallSpeed = 2.0f;

	bool wind = false;
	const float windSpeed = -1.0f;
	const float wxMaxRainX = 1.0f - windSpeed / rainFallSpeed;

	bool tide = false;
	float wySeaLevel = 1.0f;
	const float wdySeaRiseRate = 1.0f / 120.0f;

	ActionState actionState = Idle;

	CastleCell castleGrid[nyCells][nxCells];

	std::vector<olc::vi2d> ladders;
	bool nearUpLadder = false;
	bool nearDownLadder = false;

	std::vector<olc::vi2d> sunburnLocations;
	std::vector<float> sunburnTimes;

	bool debug = false;

	void drawCrenel(int sx, int sy) {
		// fill one extra line dark yellow height to overwrite "lid" of the block
		FillRect(sx + 1, sy - syCrenelHeight, sxCrenelWidth - 2, syCrenelHeight + 1, olc::DARK_YELLOW);
		DrawLine(sx, sy - syCrenelHeight, sx, sy - 1, olc::VERY_DARK_YELLOW);
		DrawLine(sx + sxCrenelWidth- 1, sy - syCrenelHeight, sx + sxCrenelWidth - 1, sy - 1, olc::VERY_DARK_YELLOW);
		DrawLine(sx, sy - syCrenelHeight, sx + sxCrenelWidth - 1, sy - syCrenelHeight, olc::VERY_DARK_YELLOW);
	}

	BucketState bucket = BucketEmpty;

	float sunburnEventRate = 1.0f/30.0f;
	float sunburnEventCharge = 0.0f;

public:
	bool OnUserCreate() override
	{
		for (int y = 0; y < nyCells; y++) {
			for (int x = 0; x < nxCells; x++) {
				castleGrid[y][x] = Empty;
			}
		}

		return true;
	}

	bool OnUserUpdate(float fElapsedTime) override
	{
		Clear(olc::CYAN);

		// sunburn event timer
		sunburnEventCharge += fElapsedTime*sunburnEventRate;
		if (sunburnEventCharge >= 1.0f) {
			// apply sunburn
			for (int x = 0; x < nxCells; x++) {
				for (int y = 0; y < nyCells; y++) {
					if (castleGrid[y][x] == Complete) {
						bool inBurns = std::find(
							sunburnLocations.begin(),
							sunburnLocations.end(),
							olc::vi2d(x, y)
						) != sunburnLocations.end();
						if (!inBurns) {
							sunburnLocations.push_back(olc::vi2d(x, y));
							sunburnTimes.push_back(sunburnTime);
						}
					}
				}
			}
			while (sunburnEventCharge >= 1.0f) {
				sunburnEventCharge -= 1.0f;
			}
		}

		// sunburn timer
		for (int n = 0; n < sunburnLocations.size();) {
			sunburnTimes[n] -= fElapsedTime;
			if (sunburnTimes[n] <= 0.0f) {
				olc::vi2d cell = sunburnLocations[n];
				
				castleGrid[cell.y][cell.x] = Empty; // dry out cell here, removing now for placeholder
				for (int l = 0; l < ladders.size(); l++) {
					if (ladders[l] == cell) {
						ladders.erase(ladders.begin() + l);
						break;
					}
				}

				sunburnLocations.erase(sunburnLocations.begin() + n);
				sunburnTimes.erase(sunburnTimes.begin() + n);
			}
			else {
				n++;
			}
		}

		if (GetKey(olc::Key::LEFT).bHeld) sxPlayerX -= sxPlayerSpeed * fElapsedTime;
		if (GetKey(olc::Key::RIGHT).bHeld) sxPlayerX += sxPlayerSpeed * fElapsedTime;
		if (GetKey(olc::Key::UP).bHeld && nearUpLadder) syPlayerY -= syPlayerSpeed * fElapsedTime;
		if (GetKey(olc::Key::DOWN).bHeld && nearDownLadder) syPlayerY += syPlayerSpeed * fElapsedTime;

		// falling
		int cxPlayerLeftX = floor((sxPlayerX - sxCellsOffset) / sxCellWidth);
		int cxPlayerRightX = floor((sxPlayerX + sxPlayerWidth - 1 - sxCellsOffset) / sxCellWidth);
		int cyInterPlayerY = floor(syPlayerY / syCellHeight);
		bool leftCanStand = castleGrid[cyInterPlayerY + 1][cxPlayerLeftX] == Complete;
		bool rightCanStand = castleGrid[cyInterPlayerY + 1][cxPlayerRightX] == Complete;
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
		int cxPlayerX = floor((sxPlayerX + sxPlayerWidth / 2 - sxCellsOffset) / sxCellWidth);
		int cyPlayerY = floor(syPlayerY / syCellHeight);

		bool inBounds = cxPlayerX >= 0 && cxPlayerX < nxCells&& cyPlayerY >= 0 && cyPlayerY < nyCells;
		bool nearTree = (cxPlayerX == -1 && cyPlayerY == nyCells - 1);
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
		nearDownLadder = ladderInCell || (ladderInBelowCell && onCellFloor);

		actionState = Idle;
		if (GetKey(olc::Key::S).bPressed && inBounds) actionState = GettingSand;
		if (GetKey(olc::Key::W).bPressed && inBounds) actionState = GettingWater;
		if (GetKey(olc::Key::C).bPressed && nearTree) actionState = GettingWood;
		if (GetKey(olc::Key::D).bPressed && inBounds) {
			switch (bucket) {
			case BucketSand:
				actionState = PouringSand;
				break;
			case BucketWater:
				actionState = PouringWater;
				break;
			case BucketWood:
				actionState = SettingLadder;
				break;
			}
		}

		if (GetKey(olc::Key::R).bPressed) raining = !raining;
		if (GetKey(olc::Key::B).bPressed) wind = !wind;
		if (GetKey(olc::Key::T).bPressed) tide = !tide;
		if (GetKey(olc::Key::G).bPressed) debug = !debug;

		if (tide) wySeaLevel -= wdySeaRiseRate * fElapsedTime;

		switch (actionState) {
		case GettingSand:
			bucket = BucketSand;
			break;
		case GettingWater:
			bucket = BucketWater;
			break;
		case GettingWood:
			bucket = BucketWood;
			break;
		case PouringSand:
			switch (castleGrid[cyPlayerY][cxPlayerX]) {
			case Empty:
				castleGrid[cyPlayerY][cxPlayerX] = HalfDry;
				bucket = BucketEmpty;
				break;
			case HalfDamp:
				castleGrid[cyPlayerY][cxPlayerX] = TopDry;
				bucket = BucketEmpty;
				break;
			}
			break;
		case PouringWater:
			switch (castleGrid[cyPlayerY][cxPlayerX]) {
			case HalfDry:
				castleGrid[cyPlayerY][cxPlayerX] = HalfDamp;
				bucket = BucketEmpty;
				break;
			case TopDry:
				castleGrid[cyPlayerY][cxPlayerX] = Complete;
				bucket = BucketEmpty;
			case Complete:
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
		case SettingLadder:
			switch (castleGrid[cyPlayerY][cxPlayerX]) {
			case Complete:
				ladders.push_back({cxPlayerX, cyPlayerY});
				bucket = BucketEmpty;
			}
			break;
		}

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

		// draw castle area
		for (int y = 0; y < nyCells; y++) {
			for (int x = 0; x < nxCells; x++) {
				switch (castleGrid[y][x]) {
				case Empty:
					break;
				case HalfDry:
					FillRect(sxCellsOffset + sxCellWidth * x, syCellHeight * y + syHalfcellHeight, sxCellWidth, syHalfcellHeight, olc::YELLOW);
					break;
				case HalfDamp:
					FillRect(sxCellsOffset + sxCellWidth * x, syCellHeight * y + syHalfcellHeight, sxCellWidth, syHalfcellHeight, olc::DARK_YELLOW);
					break;
				case TopDry:
					FillRect(sxCellsOffset + sxCellWidth * x, syCellHeight * y + syHalfcellHeight, sxCellWidth, syHalfcellHeight, olc::DARK_YELLOW);
					FillRect(sxCellsOffset + sxCellWidth * x, syCellHeight * y, sxCellWidth, syHalfcellHeight, olc::YELLOW);
					break;
				case Complete:
					FillRect(sxCellsOffset + sxCellWidth * x, syCellHeight * y, sxCellWidth, syCellHeight, olc::DARK_YELLOW);
					DrawLine(sxCellsOffset + sxCellWidth * x, syCellHeight * y, sxCellsOffset + sxCellWidth * (x + 1) - 1, syCellHeight * y, olc::VERY_DARK_YELLOW);
					break;
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

		// draw tree
		FillRect(0, syBeachMax - syCellHeight, sxCellsOffset, syCellHeight, brown);

		// draw crenellations above player
		for (int y = 0; y < cyPlayerY + 1; y++) {
			for (int x = 0; x < nxCells; x++) {
				if (castleGrid[y][x] == Complete) {
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
				if (castleGrid[y][x] == Complete) {
					drawCrenel(sxCellsOffset + sxCrenelOffset + sxCellWidth * x, syCellHeight * y);
					drawCrenel(sxCellsOffset + 3 * sxCrenelOffset + sxCrenelWidth + sxCellWidth * x, syCellHeight * y);
				}
			}
		}

		// draw sea
		int sySeaLevel = wySeaLevel * syScreenHeight;
		FillRect(0, sySeaLevel, sxScreenWidth, syScreenHeight - sySeaLevel, olc::BLUE);

		// rainfall
		if (wind) {
			for (auto& x : wxRaindropsX) {
				x += windSpeed * fElapsedTime;
			}
		}
		for (auto& y : wyRaindropsY) {
			y += rainFallSpeed*fElapsedTime;
		}
		for (int n = 0; n < wxRaindropsX.size(); n++) {
			if (wyRaindropsY[n] > wySeaLevel || wxRaindropsX[n] < 0) {
				wxRaindropsX.erase(wxRaindropsX.begin() + n);
				wyRaindropsY.erase(wyRaindropsY.begin() + n);
			}
		}
		if (raining) {
			rainCharge += fElapsedTime*rainRate;
			while (rainCharge >= 1.0f) {
				float wxNewX = wxMaxRainX * std::rand() / float(RAND_MAX);
				int sxNewX = floor(sxScreenWidth * wxNewX);
				wxRaindropsX.push_back(wxNewX);
				wyRaindropsY.push_back(0.0);
				rainCharge -= 1.0f;
			}
		}
		for (int n = 0; n < wxRaindropsX.size(); n++) {
			float wx = wxRaindropsX[n];
			float wy = wyRaindropsY[n];
			Draw(wx*sxScreenWidth, wy*syScreenHeight, olc::BLUE);
		}

		// draw hotter sun if any blocks burning
		if (sunburnLocations.size() > 0) {
			FillCircle(sxScreenWidth - 10, 10, 10, olc::RED);
		}
		else {
			FillCircle(sxScreenWidth - 10, 10, 10, olc::YELLOW);
		}

		// draw bucket UI
		FillRect(sxScreenWidth - 10, syBeachMax + 1, 10, syScreenHeight - syBeachMax - 1, olc::GREY);
		switch (bucket) {
		case BucketEmpty:
			FillRect(sxScreenWidth - 9, syBeachMax + 1, 8, syScreenHeight - syBeachMax - 2, olc::CYAN);
			break;
		case BucketSand:
			FillRect(sxScreenWidth - 9, syBeachMax + 1, 8, syScreenHeight - syBeachMax - 2, olc::DARK_YELLOW);
			break;
		case BucketWater:
			FillRect(sxScreenWidth - 9, syBeachMax + 1, 8, syScreenHeight - syBeachMax - 2, olc::DARK_BLUE);
			break;
		case BucketWood:
			FillRect(sxScreenWidth - 9, syBeachMax + 1, 8, syScreenHeight - syBeachMax - 2, brown);
			break;
		}

		DrawString(0, 0, std::to_string(nearDownLadder) + std::to_string(canStand));

		return true;
	}
};


int main()
{
	Game demo;
	if (demo.Construct(sxScreenWidth, syScreenHeight, 4, 4))
		demo.Start();

	return 0;
}