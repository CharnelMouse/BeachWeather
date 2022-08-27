#define OLC_PGE_APPLICATION
#include "olcPixelGameEngine.h"

class Game : public olc::PixelGameEngine
{
public:
	Game()
	{
		sAppName = "Beach Weather";
	}

private:
	int nxCells = 20;
	int nyCells = 20;
	float wyCellsEnd = 0.85;

	float wxPlayerX = 0.5;
	float wyPlayerY = 0.9;
	float wxPlayerWidth = 1.0 / 20.0;
	float wyPlayerHeight = 1.0 / 10.0;
	int cxPlayerX;
	int cyPlayerY;

	float wyBeachMin = 0.8;
	float wyBeachMax = 0.9;

	float wySeaLevel = 1.0;
	float wdySeaRiseRate = 1.0/120.0;

public:
	bool OnUserCreate() override
	{
		return true;
	}

	bool OnUserUpdate(float fElapsedTime) override
	{
		Clear(olc::CYAN);

		if (GetKey(olc::Key::LEFT).bHeld) wxPlayerX -= 0.2f*fElapsedTime;
		if (GetKey(olc::Key::RIGHT).bHeld) wxPlayerX += 0.2f*fElapsedTime;
		if (GetKey(olc::Key::UP).bHeld) wyPlayerY -= 0.2f * fElapsedTime;
		if (GetKey(olc::Key::DOWN).bHeld) wyPlayerY += 0.2f * fElapsedTime;

		cxPlayerX = floor(wxPlayerX * nxCells);
		cyPlayerY = floor((wyPlayerY / wyCellsEnd) * nyCells);

		wySeaLevel -= wdySeaRiseRate * fElapsedTime;

		int syBeachMax = wyBeachMax * ScreenHeight();
		int syBeachMin = wyBeachMin * ScreenHeight();
		FillRect(0, syBeachMax, ScreenWidth(), ScreenHeight() - syBeachMax, olc::YELLOW);
		FillTriangle(0, syBeachMax, 0, syBeachMin, ScreenWidth() - 1, syBeachMax, olc::YELLOW);

		for (int cx = 0; cx < nxCells; cx++) {
			int sx = cx * ScreenWidth() / nxCells;
			DrawLine(sx, 0, sx, wyCellsEnd * ScreenHeight(), olc::GREY);
		}
		for (int cy = 0; cy <= nyCells; cy++) {
			int sy = cy * ScreenWidth() * wyCellsEnd / nyCells;
			DrawLine(0, sy, ScreenWidth() - 1, sy, olc::GREY);
		}

		int sySeaLevel = wySeaLevel * ScreenHeight();
		FillRect(0, sySeaLevel, ScreenWidth(), ScreenHeight() - sySeaLevel, olc::BLUE);

		int sxPlayerX = round(wxPlayerX * ScreenWidth());
		int syPlayerY = round(wyPlayerY * ScreenHeight());
		int sxPlayerWidth = round(wxPlayerWidth * ScreenWidth());
		int syPlayerHeight = round(wyPlayerHeight * ScreenHeight());
		FillRect(sxPlayerX, syPlayerY, sxPlayerWidth, syPlayerHeight, olc::MAGENTA);

		DrawString(0, 0, std::to_string(wySeaLevel));

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