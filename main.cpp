#include <iostream>
#include <vector>
#include <utility>
#include <algorithm>
#include <chrono>
#include <stdio.h>
#include <Windows.h>
using namespace std;

constexpr uint16_t nScreenWidth = 200;        //Console Screen Size X (columns)
constexpr uint16_t nScreenHeight = 75;        //Console Screen Size Y (rows)
constexpr uint16_t nMapWidth = 16;            //World Dimensions
constexpr uint16_t nMapHeight = 16;

constexpr float_t fFOV = 3.141592741F / 4.F; //Field of View
constexpr float_t fDepth = 32.0F;             //Maximum rendering distance
constexpr float_t fSpeed = 2.5F;              //Walking Speed
float_t fPlayerX = 14.7F;                     //Player Start Position
float_t fPlayerY = 5.09F;
float_t fPlayerA = 0.F;                      //Player Start Rotation

int main() {
  // Create Screen Buffer
  _wsystem(L"mode con lines=75 cols=200");
  SetConsoleTitleW(L"ConsoleFPS");

  wchar_t* screen = new wchar_t[nScreenWidth * nScreenHeight];
  HANDLE hConsole = CreateConsoleScreenBuffer(GENERIC_READ | GENERIC_WRITE, 0, NULL, CONSOLE_TEXTMODE_BUFFER, NULL);
  SetConsoleActiveScreenBuffer(hConsole);
  DWORD dwBytesWritten = 0;

  // Create Map of world space # = wall block, . = space
  const wstring map = \
    L"################"\
    L"#..............#"\
    L"#..............#"\
    L"#..............#"\
    L"#########......#"\
    L"#..............#"\
    L"#..######......#"\
    L"#.#............#"\
    L"##.............#"\
    L"#......####..###"\
    L"#......#.......#"\
    L"#......#.......#"\
    L"#..............#"\
    L"#......#########"\
    L"#..............#"\
    L"################";

  chrono::system_clock::time_point tp1;
  chrono::system_clock::time_point tp2;
  chrono::duration<float_t> elapsedTime;
  while(true) {
    //We'll need time differential per frame to calculate modification
    //to movement speeds, to ensure consistant movement, as ray-tracing
    //is non-deterministic
    tp2 = chrono::system_clock::now();
    elapsedTime = tp2 - tp1;
    tp1 = tp2;
    float_t fElapsedTime = elapsedTime.count();

    //Handle CCW Rotation
    if(GetAsyncKeyState((uint16_t)'A') & 0x8000)
      fPlayerA -= (fSpeed * 0.75f) * fElapsedTime;

    //Handle CW Rotation
    if(GetAsyncKeyState((uint16_t)'D') & 0x8000)
      fPlayerA += (fSpeed * 0.75f) * fElapsedTime;

    //Handle Forwards movement & collision
    if(GetAsyncKeyState((uint16_t)'W') & 0x8000) {
      fPlayerX += sinf(fPlayerA) * fSpeed * fElapsedTime;;
      fPlayerY += cosf(fPlayerA) * fSpeed * fElapsedTime;;
      if(map.c_str()[(int)fPlayerX * nMapWidth + (int)fPlayerY] == '#') {
        fPlayerX -= sinf(fPlayerA) * fSpeed * fElapsedTime;;
        fPlayerY -= cosf(fPlayerA) * fSpeed * fElapsedTime;;
      }
    }

    //Handle backwards movement & collision
    if(GetAsyncKeyState((uint16_t)'S') & 0x8000) {
      fPlayerX -= sinf(fPlayerA) * fSpeed * fElapsedTime;;
      fPlayerY -= cosf(fPlayerA) * fSpeed * fElapsedTime;;
      if(map.c_str()[(int)fPlayerX * nMapWidth + (int)fPlayerY] == '#') {
        fPlayerX += sinf(fPlayerA) * fSpeed * fElapsedTime;;
        fPlayerY += cosf(fPlayerA) * fSpeed * fElapsedTime;;
      }
    }

    for(uint16_t x = 0; x < nScreenWidth; x++) {
      //For each column, calculate the projected ray angle into world space
      float_t fRayAngle = (fPlayerA - fFOV / 2.0f) + ((float_t)x / (float_t)nScreenWidth) * fFOV;

      // Find distance to wall
      constexpr float_t fStepSize = 0.05f;  //Increment size for ray casting, decrease to increase resolution
      float_t fDistanceToWall = 0.0f;

      bool bHitWall = false;  //Set when ray hits wall block
      bool bBoundary = false; //Set when ray hits boundary between two wall blocks

      float_t fEyeX = sinf(fRayAngle); //Unit vector for ray in player space
      float_t fEyeY = cosf(fRayAngle);

      //Incrementally cast ray from player, along ray angle, testing for 
      //intersection with a block
      while(!bHitWall && fDistanceToWall < fDepth) {
        fDistanceToWall += fStepSize;
        int16_t nTestX = (int16_t)(fPlayerX + fEyeX * fDistanceToWall);
        int16_t nTestY = (int16_t)(fPlayerY + fEyeY * fDistanceToWall);

        //Test if ray is out of bounds
        if(nTestX < 0 || nTestX >= nMapWidth || nTestY < 0 || nTestY >= nMapHeight) {
          bHitWall = true;   //Just set distance to maximum depth
          fDistanceToWall = fDepth;
        }
        else {
          //Ray is inbounds so test to see if the ray cell is a wall block
          if(map[nTestX * nMapWidth + nTestY] == L'#') {
            //Ray has hit wall
            bHitWall = true;

            //To highlight tile boundaries, cast a ray from each corner
            //of the tile, to the player. The more coincident this ray
            //is to the rendering ray, the closer we are to a tile 
            //boundary, which we'll shade to add detail to the walls
            std::vector<std::pair<float_t, float_t>> p;

            //Test each corner of hit tile, storing the distance from
            //the player, and the calculated dot product of the two rays
            for(uint8_t tx = 0; tx < 2; ++tx) {
              for(uint8_t ty = 0; ty < 2; ++ty) {
                //Angle of corner to eye
                float_t vy = (float_t)nTestY + ty - fPlayerY;
                float_t vx = (float_t)nTestX + tx - fPlayerX;
                float_t d = sqrt(vx * vx + vy * vy);
                float_t dot = (fEyeX * vx / d) + (fEyeY * vy / d);
                p.push_back(make_pair(d, dot));
              }
            }
            //Sort Pairs from closest to farthest
            sort(p.begin(), p.end(), [](const pair<float_t, float_t>& left, const pair<float_t, float_t>& right) {return left.first < right.first; });

            //First two/three are closest (we will never see all four)
            constexpr float_t fBound = 0.01;
            if(acos(p.at(0).second) < fBound) bBoundary = true;
            if(acos(p.at(1).second) < fBound) bBoundary = true;
            if(acos(p.at(2).second) < fBound) bBoundary = true;
          }
        }
      }

      //Calculate distance to ceiling and floor
      int16_t nCeiling = (float_t)(nScreenHeight / 2.0) - nScreenHeight / ((float_t)fDistanceToWall);
      int16_t nFloor = nScreenHeight - nCeiling;

      //Shader walls based on distance
      wchar_t nShade = ' ';
      if(bBoundary) {
        nShade = ' '; //Black it out
      }
      else {
        if(fDistanceToWall <= fDepth / 4.0f) {
          nShade = 0x2588;
        }	// Very close	
        else if(fDistanceToWall < fDepth / 3.0f) {
          nShade = 0x2593;
        }
        else if(fDistanceToWall < fDepth / 2.0f) {
          nShade = 0x2592;
        }
        else if(fDistanceToWall < fDepth) {
          nShade = 0x2591;
        }
        else {
          nShade = ' ';		//Too far away
        }
      }

      for(uint16_t y = 0; y < nScreenHeight; ++y) {
        //Each Row
        if(y <= nCeiling) {
          screen[y * nScreenWidth + x] = ' ';
        }
        else if(y > nCeiling && y <= nFloor) {
          screen[y * nScreenWidth + x] = nShade;
        }
        else { //Floor 
          //Shade floor based on distance
          const float_t b = 1.F - (((float_t)y - nScreenHeight / 2.F) / ((float_t)nScreenHeight / 2.F));
          if(b < 0.25)		nShade = '#';
          else if(b < 0.5)	nShade = 'x';
          else if(b < 0.75)	nShade = '.';
          else if(b < 0.9)	nShade = '-';
          else				nShade = ' ';
          screen[y * nScreenWidth + x] = nShade;
        }
      }
    }

    //Display Stats
    swprintf_s(screen, 80, L"X=%3.2f, Y=%3.2f, A=%3.2f FPS=%3.2f ", fPlayerX, fPlayerY, fPlayerA, 1.F / fElapsedTime);

    //Display Map
    for(uint8_t nx = 0; nx < nMapWidth; nx++) {
      for(uint8_t ny = 0; ny < nMapWidth; ny++) {
        screen[(ny + 1) * nScreenWidth + nx] = map[static_cast<uint64_t>(ny) * nMapWidth + nx];
      }
    }
    screen[((uint64_t)fPlayerX + 1) * nScreenWidth + (uint64_t)fPlayerY] = 'P';

    //Display Frame
    screen[nScreenWidth * nScreenHeight - 1] = '\0';
    WriteConsoleOutputCharacterW(hConsole, screen, nScreenWidth * nScreenHeight, {0, 0}, &dwBytesWritten);
  }

  return 0;
}
