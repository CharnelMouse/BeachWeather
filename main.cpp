#define OLC_PGE_APPLICATION
#include "olcPixelGameEngine.h"

static const int nxCells = 120;
static const int nyCells = 108;

enum CastlePixel {
	Empty,
	DrySand,
	Water,
	DampSand
};
void swap(CastlePixel grid[nyCells][nxCells], int x1, int y1, int x2, int y2) {
	CastlePixel t1 = grid[y1][x1];
	CastlePixel t2 = grid[y2][x2];
	grid[y2][x2] = t1;
	grid[y1][x1] = t2;
}

class Game : public olc::PixelGameEngine
{
public:
	Game()
	{
		sAppName = "Beach Weather";
	}

private:
	static const int syBeachMin = 96; // 0.8 of screen
	static const int syBeachMax = 108; // 0.9 of screen

	static const int sxCellWidth = 120/nxCells;
	static const int syCellHeight = syBeachMax/nyCells;

	const float sxPlayerWidth = 2.0 * sxCellWidth;
	const float syPlayerHeight = 4.0 * syCellHeight;
	float sxPlayerX = 60;
	float syPlayerY = syBeachMax - syPlayerHeight;
	int cxPlayerX;
	int cyPlayerY;

	bool raining = false;
	float rainCharge = 0.0;
	std::vector<float> wxRaindropsX;
	std::vector<float> wyRaindropsY;
	const float rainRate = 50.0;
	const float rainFallSpeed = 2.0;
	float rainParticleCharge = 0.0;
	const float rainParticleRate = 10.0;

	bool wind = false;
	const float windSpeed = -1.0;
	const float wxMaxRainX = 1.0 - windSpeed / rainFallSpeed;

	bool tide = false;
	float wySeaLevel = 1.0;
	const float wdySeaRiseRate = 1.0 / 120.0;

	bool pouringSand;
	float sandCharge = 0.0;
	const float sandRate = 20.0;

	bool pouringWater;
	float waterCharge = 0.0;
	const float waterRate = 20.0;

	bool pouringDampSand;
	float dampSandCharge = 0.0;
	const float dampSandRate = 20.0;

	CastlePixel castleArea[nyCells][nxCells];

	float fallCharge = 0.0;
	const float fallRate = 40.0;

	bool debug = false;

public:
	bool OnUserCreate() override
	{
		for (int y = 0; y < nyCells; y++) {
			for (int x = 0; x < nxCells; x++) {
				castleArea[y][x] = Empty;
			}
		}

		return true;
	}

	bool OnUserUpdate(float fElapsedTime) override
	{
		Clear(olc::CYAN);

		if (GetKey(olc::Key::LEFT).bHeld) sxPlayerX -= 24.0f * fElapsedTime;
		if (GetKey(olc::Key::RIGHT).bHeld) sxPlayerX += 24.0f * fElapsedTime;
		if (GetKey(olc::Key::UP).bHeld) syPlayerY -= 24.0f * fElapsedTime;
		if (GetKey(olc::Key::DOWN).bHeld) syPlayerY += 24.0f * fElapsedTime;
		pouringSand = (GetKey(olc::Key::S).bHeld);
		pouringWater = (GetKey(olc::Key::W).bHeld);
		pouringDampSand = (GetKey(olc::Key::D).bHeld);

		if (GetKey(olc::Key::R).bPressed) raining = !raining;
		if (GetKey(olc::Key::B).bPressed) wind = !wind;
		if (GetKey(olc::Key::T).bPressed) tide = !tide;
		if (GetKey(olc::Key::G).bPressed) debug = !debug;

		syPlayerY = std::max(std::min(syPlayerY, float(syBeachMax - syPlayerHeight)), 0.0f);

		cxPlayerX = floor((sxPlayerX + sxPlayerWidth/2) / sxCellWidth);
		cyPlayerY = floor(syPlayerY / syCellHeight);

		if (tide) wySeaLevel -= wdySeaRiseRate * fElapsedTime;

		if (pouringSand && cxPlayerX >= 0 && cxPlayerX < nxCells && cyPlayerY >= 0 && cyPlayerY < nyCells) {
			sandCharge += fElapsedTime * sandRate;
			if (sandCharge >= 1.0) {
				castleArea[cyPlayerY][cxPlayerX] = DrySand;
				sandCharge = 0.0;
			}
		}
		else {
			sandCharge = 0.0;
		}

		if (pouringWater && cxPlayerX >= 0 && cxPlayerX < nxCells && cyPlayerY >= 0 && cyPlayerY < nyCells) {
			waterCharge += fElapsedTime * waterRate;
			if (waterCharge >= 1.0) {
				castleArea[cyPlayerY][cxPlayerX] = Water;
				waterCharge = 0.0;
			}
		}
		else {
			waterCharge = 0.0;
		}

		if (pouringDampSand && cxPlayerX >= 0 && cxPlayerX < nxCells && cyPlayerY >= 0 && cyPlayerY < nyCells) {
			dampSandCharge += fElapsedTime * dampSandRate;
			if (dampSandCharge >= 1.0) {
				castleArea[cyPlayerY][cxPlayerX] = DampSand;
				dampSandCharge = 0.0;
			}
		}
		else {
			dampSandCharge = 0.0;
		}

		// apply gravity
		fallCharge += fElapsedTime * fallRate;
		if (fallCharge >= 1.0) {
			for (int y = nyCells - 1; y >= 0; y--) {
				for (int x = 0; x < nxCells; x++) {
					switch (castleArea[y][x]) {
					case DrySand:
						if (y < nyCells - 1) {
							CastlePixel below = castleArea[y + 1][x];
							switch (below) {
							case Empty:
								swap(castleArea, x, y, x, y + 1);
								break;
							case Water:
								castleArea[y + 1][x] = DampSand;
								castleArea[y][x] = Empty;
								break;
							case DrySand:
							case DampSand:
								if (y % 2 == 0) {
									if (x > 0 && castleArea[y + 1][x - 1] == Empty) {
										swap(castleArea, x, y, x - 1, y + 1);
									}
									else {
										if (x < nxCells - 1 && castleArea[y + 1][x + 1] == Empty) {
											swap(castleArea, x, y, x + 1, y + 1);
										}
									}
								}
								else {
									if (x < nxCells - 1 && castleArea[y + 1][x + 1] == Empty) {
										swap(castleArea, x, y, x + 1, y + 1);
									}
									else {
										if (x > 0 && castleArea[y + 1][x - 1] == Empty) {
											swap(castleArea, x, y, x - 1, y + 1);
										}
									}
								}
								break;
							}
						}
						break;
					case Water:
						if (y < nyCells - 1) {
							CastlePixel below = castleArea[y + 1][x];
							switch (below) {
							case Empty:
								swap(castleArea, x, y, x, y + 1);
								break;
							case DrySand:
								castleArea[y + 1][x] = DampSand;
								castleArea[y][x] = Empty;
							case DampSand:
							case Water:
								if (y % 2 == 0) {
									if (x > 0 && castleArea[y + 1][x - 1] == Empty) {
										swap(castleArea, x, y, x - 1, y + 1);
									}
									else {
										if (x < nxCells - 1 && castleArea[y + 1][x + 1] == Empty) {
											swap(castleArea, x, y, x + 1, y + 1);
										}
										else {
											if (x > 0 && castleArea[y][x - 1] == Empty) {
												swap(castleArea, x, y, x - 1, y);
											}
											else {
												if (x < nxCells - 1 && castleArea[y][x + 1] == Empty) {
													swap(castleArea, x, y, x + 1, y);
												}
											}
										}
									}
								}
								else {
									if (x < nxCells - 1 && castleArea[y + 1][x + 1] == Empty) {
										swap(castleArea, x, y, x + 1, y + 1);
									}
									else {
										if (x > 0 && castleArea[y + 1][x - 1] == Empty) {
											swap(castleArea, x, y, x - 1, y + 1);
										}
										else {
											if (x < nxCells - 1 && castleArea[y][x + 1] == Empty) {
												swap(castleArea, x, y, x + 1, y);
											}
											else {
												if (x > 0 && castleArea[y][x - 1] == Empty) {
													swap(castleArea, x, y, x - 1, y);
												}
											}
										}
									}
								}
								break;
							}
						}
						break;
					case DampSand:
						if (y < nyCells - 1) {
							CastlePixel below = castleArea[y + 1][x];
							switch (below) {
							case Empty:
							case Water:
								swap(castleArea, x, y, x, y + 1);
								break;
							case DrySand:
							case DampSand:
								break;
							}
						}
						break;
					case Empty:
						break;
					}
				}
			}
			fallCharge -= 1.0;
		}

		// draw beach
		FillRect(0, syBeachMax, ScreenWidth(), ScreenHeight() - syBeachMax, olc::DARK_YELLOW);

		// draw castle area
		for (int y = 0; y < nyCells; y++) {
			for (int x = 0; x < nxCells; x++) {
				CastlePixel p = castleArea[y][x];
				switch (castleArea[y][x]) {
				case DrySand: FillRect(x * sxCellWidth, y * syCellHeight, sxCellWidth, syCellHeight, olc::YELLOW); break;
				case Water: FillRect(x * sxCellWidth, y * syCellHeight, sxCellWidth, syCellHeight, olc::BLUE); break;
				case DampSand: FillRect(x * sxCellWidth, y * syCellHeight, sxCellWidth, syCellHeight, olc::DARK_YELLOW); break;
				}
			}
		}

		// draw sea
		int sySeaLevel = wySeaLevel * ScreenHeight();
		FillRect(0, sySeaLevel, ScreenWidth(), ScreenHeight() - sySeaLevel, olc::BLUE);

		// draw debug grid lines
		if (debug) {
			for (int cx = 0; cx < nxCells; cx++) {
				int sx = cx * sxCellWidth;
				DrawLine(sx, 0, sx, syBeachMax, olc::GREY);
			}
			for (int cy = 0; cy <= nyCells; cy++) {
				int sy = cy * syCellHeight;
				DrawLine(0, sy, ScreenWidth() - 1, sy, olc::GREY);
			}
		}

		// draw player
		FillRect(sxPlayerX, syPlayerY, sxPlayerWidth, syPlayerHeight, olc::MAGENTA);

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
			while (rainCharge >= 1) {
				float wxNewX = wxMaxRainX * std::rand() / float(RAND_MAX);
				int sxNewX = floor(ScreenWidth() * wxNewX);
				wxRaindropsX.push_back(wxNewX);
				wyRaindropsY.push_back(0.0);
				rainCharge -= 1.0;
			}
			rainParticleCharge += fElapsedTime * rainParticleRate;
			while (rainParticleCharge >= 1) {
				float wxNewX = wxMaxRainX * std::rand() / float(RAND_MAX);
				int sxNewX = floor(ScreenWidth() * wxNewX);
				castleArea[0][int(sxNewX / sxCellWidth)] = Water;
				rainParticleCharge -= 1.0;
			}
		}
		for (int n = 0; n < wxRaindropsX.size(); n++) {
			float wx = wxRaindropsX[n];
			float wy = wyRaindropsY[n];
			Draw(wx*ScreenWidth(), wy*ScreenHeight(), olc::BLUE);
		}

		DrawString(0, 0, std::to_string(cxPlayerX) + ", " + std::to_string(cyPlayerY));

		return true;
	}
};


int main()
{
	Game demo;
	if (demo.Construct(120, 120, 4, 4))
		demo.Start();

	return 0;
}