#include <stdio.h>
#include <stdlib.h>
#include <conio.h>
#include <pthread.h>
#include <semaphore.h>
#include <windowsx.h>
#include <windows.h>
#include <time.h>
#include <math.h>

#define SHIP_SPEED 100
#define PLANET_RADIUS 30
// hwnds
static HWND hwnd; /* A 'HANDLE', hence the H, or a pointer to our window */
static HWND startWindow;
static HWND consoleBox;
static HWND hListBox;


// Logs
char logMessage[128];
//void toConsole(char []);

// ******* Stations logic start *******
typedef struct {
	int ID;
	int WaitedPassengers[100];  // Cause in worst situation
								//all citizens can be on one station (20*5)
	int X;
	int Y;
	pthread_mutex_t Port_mut;
	pthread_mutex_t Queue_mut;
	char Name[9];
}Station;

Station stations[5] = {
	{0, {0,1,2,3}, 122,465, NULL,NULL, {'A', 'l', 't', 'a', 'i', 'r'}},
	{1, {0,1,2,3}, 386,465, NULL,NULL, {'C','a','n','o','p','u','s'}},
	{2, {0,1,2,3}, 465,215, NULL,NULL, {'C','a','p','e','l','l','a'}},
	{3, {0,1,2,3}, 40,215,  NULL,NULL, {'N','e','w','-','T','e','r','r','a'}},
	{4, {0,1,2,3}, 255,60,  NULL,NULL, {'E','a','r','t','h'}}
//	,{}
};


void DrawPlanets(HDC hdc){
	// Solid pen for planet
	HPEN pen = CreatePen(PS_SOLID, 2, 0);
	SelectObject(hdc, pen);
	// Draw planets and they names
	int i=0;
	for(; i<5; i=i+1){
		int x = stations[i].X;
		int y = stations[i].Y;

		Ellipse(hdc, x-PLANET_RADIUS, y-PLANET_RADIUS, x+PLANET_RADIUS, y+PLANET_RADIUS);
		TextOut(hdc, x+PLANET_RADIUS/3, y-PLANET_RADIUS*1.7, stations[i].Name,9);
//		snprintf(logMessage, 128, "%s planet space port is prepared for a ship", stations[i].Name);
//		toConsole(logMessage);
	}
	DeleteObject(pen);
}

void DrawRoutes(HDC hdc){
	HPEN pen = CreatePen(PS_DASH, 1, 0);
	SelectObject(hdc, pen);
	// Draw routes
	int i=0;
	for(; i<5; i=i+1){
		int j=4;
		for(;j>i;j=j-1){
			MoveToEx(hdc, stations[i].X, stations[i].Y, 0);
			LineTo(hdc, stations[j].X, stations[j].Y);
		}
	}
	DeleteObject(pen);
}

// ******* Stations logic end *******

// ******* Ships logic start *******
pthread_t threads_ships[3];

typedef struct{
	char Name[3];
	// sem_t FreeSpaceSem;
	short State;
	Station *Dest;
	double Direction;
	int X,Y;
}Ship;

Ship ships[] = {
	{ {'I', 'N', 'K'}, 1, &stations[0], 	0.7, 150, 200},
	{ {'D', 'E', 'F'}, 1, &stations[1], 	-1.3,200, 100},
	{ {'A', 'R', 'K'}, 1, &stations[2], 	0, 	  32, 300}
};

void ship_nextDest(Ship *ship, int next_dest){
	int x1 = ship->X;
	int y1 = ship->Y;
	int x2 = stations[next_dest].X;
	int y2 = stations[next_dest].Y;

	ship->Direction = atan2((y2-y1),(x2-x1));
	ship->Dest = &stations[next_dest];
}

void* ship_modeling(void *arg){
	Ship *ship_m = (Ship*) arg;
	while(1){
		// TODO DELETE THIS
		int next;
		do{
			Sleep(SHIP_SPEED);
			srand(time(NULL) + (int)ship_m->Name[1] );
			next = rand() % 5;
		}while(next==ship_m->Dest->ID);
		ship_nextDest(ship_m, next);
		// TODO change next
		// TODO DELETE THIS
		
		int dx = (int)(cos(ship_m->Direction)*10);
		int dy = (int)(sin(ship_m->Direction)*10);
		
		int stX = ship_m->Dest->X;
		int stY = ship_m->Dest->Y;
		
		// Flying
		while( !(	((stX+PLANET_RADIUS > ship_m->X)&(stX-PLANET_RADIUS < ship_m->X)) &
					((stY+PLANET_RADIUS > ship_m->Y)&(stY-PLANET_RADIUS < ship_m->Y)) ))
		{
			ship_m->X = ship_m->X+dx;
			ship_m->Y = ship_m->Y+dy;
			InvalidateRect(hwnd, NULL, FALSE);
			Sleep(SHIP_SPEED);
		}
		
		//TODO delete promts
		pthread_mutex_lock(&ship_m->Dest->Port_mut);
			snprintf(logMessage, 128, "Ship <%s> lock mutex of #<%s>", 
						ship_m->Name,ship_m->Dest->Name);
			toConsole(logMessage);
			ship_m->X = ship_m->Dest->X;
			ship_m->Y = ship_m->Dest->Y;
			Sleep(3000);
		pthread_mutex_unlock(&ship_m->Dest->Port_mut);
		snprintf(logMessage, 128, "Ship <%s> unlock mutex of #<%s>", 
						ship_m->Name,ship_m->Dest->Name);
		toConsole(logMessage);
	}
	return 0;
}

// ******* Ships logic end *******

void start_threads(){
	int j;
	for(j=0;j<5;j=j+1){
//		pthread_mutex_init(&anchors[j],NULL);
		pthread_mutex_init(&stations[j].Port_mut,NULL);
//		pthread_create(&threads_stations[j], NULL, station_modeling, (void*) &stations[j]);	 
	}	
	for(j=0;j<3;j=j+1)	{
		
		pthread_create(&threads_ships[j], NULL, ship_modeling, (void*) &ships[j]);	 
	}
	
}

void stop_threads(){
	int j;
	for(j=0;j<5;j=j+1){
//		pthread_mutex_destroy(&anchors[j]);
		pthread_mutex_destroy(stations[j].Port_mut);
//		pthread_exit(&threads_stations[j]);	 
	}	
	for(j=0;j<3;j=j+1)	{
		pthread_exit(&threads_ships[j]);	 
	}
}

void DrawShips (HDC hdc){
	HBRUSH brush = CreateSolidBrush(0);
	SelectObject(hdc, brush);
	short dx = 10;
	short dy = 8;
	int i=0;
	for(;i<3;i=i+1)
	{		
		int x = ships[i].X;
		int y = ships[i].Y;
		POINT triangle[] = {{ x, y-dy},  { x-dx, y+dy}, { x+dx, y+dy}};
		Polygon(hdc, triangle, 3);
		TextOut(hdc, x+10, y+10, ships[i].Name,3);
	}		
	DeleteObject(brush);
}

void DrawComponents(HDC hdc, RECT rect){
	int width = rect.right - rect.left;
	int height = rect.bottom - rect.top;

	HDC hMemDC = CreateCompatibleDC(hdc);
	HBITMAP hMemBitmap = CreateCompatibleBitmap(hdc, width, height);
	SelectObject(hMemDC, hMemBitmap);

	HBRUSH hBrush = CreateSolidBrush(RGB(255,255,255)); // Fill white visible screen
	FillRect(hMemDC,&rect,hBrush);
	DeleteObject(hBrush);
	SetTextAlign(hMemDC, TA_CENTER);
 	DrawRoutes(hMemDC);
 	DrawPlanets(hMemDC);
 	DrawShips(hMemDC);

	BitBlt(hdc, 0, 0, width, height, hMemDC, 0, 0, SRCCOPY);
	DeleteObject(hMemBitmap);
	DeleteDC(hMemDC);
}

void toConsole(char txt[],...){
	SendMessage(consoleBox, LB_INSERTSTRING, 0, (LPARAM)txt);
}

LRESULT OnCommand(HWND hwnd, int id, HWND hwndCtl, UINT codeNotify){
	switch(id){
		case 5: {break;}
	}
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT Message, WPARAM wParam, LPARAM lParam) {
	HDC hdc;
	PAINTSTRUCT ps;
	switch(Message) {
		case WM_DESTROY: {
			stop_threads();
			PostQuitMessage(0);
			break;
		}
		default: return DefWindowProc(hwnd, Message, wParam, lParam);

		case WM_TIMER: {
			if(IsIconic(hwnd))
    			return;
			break;
		}
		case WM_PAINT: {
			hdc = BeginPaint(hwnd, &ps);
			DrawComponents(hdc, ps.rcPaint);
			EndPaint(hwnd, &ps);
			break;
		}
		
		case WM_MOVE:{
			InvalidateRect(hwnd, NULL, FALSE);
			break;
		}
		case WM_ERASEBKGND: {
			return 1;
		}
		HANDLE_MSG(hwnd, WM_COMMAND, OnCommand);
	return 0;
	}
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
	WNDCLASSEX wc; /* A properties struct of our window */
	
	MSG msg; /* A temporary location for all messages */
	memset(&wc,0,sizeof(wc));
	wc.cbSize		 = sizeof(WNDCLASSEX);
	wc.lpfnWndProc	 = WndProc;
	wc.hInstance	 = hInstance;
	wc.hCursor		 = LoadCursor(NULL, IDC_ARROW);

	wc.hbrBackground = CreateSolidBrush(RGB(255,255,255));//(HBRUSH)(COLOR_WINDOW+1);
	wc.lpszClassName = "WindowClass";
	wc.hIcon		 = LoadIcon(NULL, IDI_APPLICATION);
	wc.hIconSm		 = LoadIcon(NULL, IDI_APPLICATION);

	if(!RegisterClassEx(&wc)) {
		MessageBox(NULL, "Window Registration Failed!","Error!",MB_ICONEXCLAMATION|MB_OK);
		return 0;
	}
	//main dialog
	 startWindow = CreateWindow("frame","Galactic democratic transport modeling program",WS_VISIBLE|WS_OVERLAPPED | WS_SYSMENU | WS_CLIPCHILDREN |
			WS_TILEDWINDOW | WS_VISIBLE | LBS_STANDARD,
			100, 900, 800, 360,
			hwnd,NULL,hInstance,NULL);

	//main window
	hwnd = CreateWindowEx(WS_EX_CLIENTEDGE,"WindowClass","Galactic democratic transport modeling program", WS_VISIBLE|WS_OVERLAPPED | WS_SYSMENU | WS_CLIPCHILDREN ,
		CW_USEDEFAULT, /* x */
		CW_USEDEFAULT, /* y */
		1120, /* width */
		800, /* height */
		NULL,NULL,hInstance,NULL);

	consoleBox = CreateWindow("listbox", NULL,
   			WS_CHILD | WS_VISIBLE | LBS_STANDARD | LBS_DISABLENOSCROLL |
   			LBS_WANTKEYBOARDINPUT,
   			10, 530, 500, 240,
   			hwnd, (HMENU) 2, hInstance, NULL);

	start_threads();

	while(GetMessage(&msg, NULL, 0, 0) > 0) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	return msg.wParam;
}
