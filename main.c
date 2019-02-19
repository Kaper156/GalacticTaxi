#include <stdio.h>
#include <stdlib.h>
#include <conio.h>
#include <pthread.h>
#include <semaphore.h>
#include <windowsx.h>
#include <windows.h>
#include <commctrl.h>
#include <time.h>
#include <math.h>
#include <stdarg.h>

// Settings
#define SHIP_SPEED 		10
#define STOP_DELAY 		3000
#define PASSENGER_SLEEP	100
#define PLANET_RADIUS 	30
#define MAX_TRAVELS 	1

// For test
//#define SHIP_SPEED 		100
//#define STOP_DELAY 		6000
//#define PASSENGER_SLEEP	100
//#define PLANET_RADIUS 	30
//#define MAX_TRAVELS 	1


// Constants for passenger->state
#define LANDING		3
#define FLY 		2
#define ARRIVE 		1
#define WAITING 	0
#define END_TRAVEL -1

// Timer ID's
#define TM_DRAW 		1
#define TM_PASSENGER 	2

// hwnds
static HWND hwnd; 
static HWND startWindow;
static HWND consoleBox;
static HWND listViewPassengers;

// Logs
void toConsoleSprintf (char *fmt, ...);

// ******* Structures *******
//typedef struct Station Station;
//typedef struct Ship Ship;

typedef struct Station {
	int ID;
	int X, Y;
	char Name[9];
	pthread_mutex_t Port_mut;
	int ArriveTo, ShipInPortID;
	int PasCnt;
}Station;

typedef struct Ship {
	int ID;
	char Name[3];
	Station *Dest;
	double Direction;
	double X,Y;
	int FreeSpace;
	pthread_mutex_t Queue_mut;
}Ship;

typedef struct Passenger {
	int ID, State, TrvCnt;
	Station *From;
	Station *Dest;
	Ship *Transport;
}Passenger;

// ******* Variables *******

Station stations[5];

pthread_t threads_ships[3];
Ship ships[3];

// Dynamic
int ps_cnt;
pthread_t *threads_ps;
Passenger *ps;

// ******* Modeling *******

int rand_exclude(int border, int exc, int seed){
	int temp;
	do{
		srand(time(NULL) + seed);
		temp = rand() % border;
	}while(temp == exc);
	return temp;
}

void* passenger_modeling(void *arg){
	Passenger *pas = (Passenger*) arg;
	pas->State = WAITING;
	int shipID;
	int destID;
	// Lock for thread safe
	pthread_mutex_lock(&pas->From->Port_mut);
	pas->From->PasCnt++;
	pthread_mutex_unlock(&pas->From->Port_mut);
	
	// Main loop for passenger
	while(pas->TrvCnt < MAX_TRAVELS){ 
		// Wait ship to needed station
		Sleep(STOP_DELAY*0.7); //Wait for next ship
		while((pas->State==WAITING) & (pas->From->ArriveTo != pas->Dest->ID)){
			Sleep(PASSENGER_SLEEP);
		}
		
		// Get shipID and check it
		shipID = pas->From->ShipInPortID;
		if(shipID < 0 ){
			Sleep(STOP_DELAY);
			continue;
		}
		
		pas->Transport = &ships[shipID];
		
		pthread_mutex_lock(&pas->Transport->Queue_mut);
			pas->State = ARRIVE;
			Sleep(PASSENGER_SLEEP);
			if(pas->Transport->FreeSpace < 1){
				pthread_mutex_unlock(&pas->Transport->Queue_mut);
				pas->Transport = NULL;
				continue;
			}
			pas->Transport->FreeSpace--;
			pas->From->PasCnt--; // port mutex locked while queue_mut lock
			pas->State = FLY;
			toConsoleSprintf("Passenger <%d> landing to ship <%s>", 
						pas->ID, pas->Transport->Name);
		pthread_mutex_unlock(&pas->Transport->Queue_mut);
		
		// Sleep until landing
		while(pas->State != LANDING){
			Sleep(PASSENGER_SLEEP);
		}
		
		pas->From = pas->Dest;
		toConsoleSprintf("Passenger <%d> landing to station <%s>", 
						pas->ID, pas->From->Name);
		
		pthread_mutex_lock(&pas->From->Port_mut);
		pas->From->PasCnt++;
		pthread_mutex_unlock(&pas->From->Port_mut);
		
		pas->State = WAITING;
//		Sleep(STOP_DELAY); //Wait for next ship
		
		//Generate next dest 
		destID = rand_exclude(5, pas->From->ID, pas->ID);
		pas->Dest = &stations[destID];
		
		// Add to cnt
		pas->TrvCnt++;
	}	
	pas->State = END_TRAVEL;
	pas->Dest = pas->From;

	pthread_mutex_lock(&pas->From->Port_mut);
	pas->From->PasCnt--;
	pthread_mutex_unlock(&pas->From->Port_mut);
	
//	curStation->PasCnt--;
	return 0;
}

Station* ship_nextDest(Ship *ship, int next_dest){
	int x1 = ship->X;
	int y1 = ship->Y;
	int x2 = stations[next_dest].X;
	int y2 = stations[next_dest].Y;

	ship->Direction = atan2((y2-y1),(x2-x1));
	ship->Dest = &stations[next_dest];
	return &stations[next_dest];
}

int find_max_dest(int stID){
	// Find maximum destination who passenger want
	int max = 0, ID = 0, i=0;
	int cnts[5] = {0,0,0,0,0};
	
	for(i=0; i<ps_cnt; i=i+1){ //count dests
		if((ps[i].State==WAITING) & (ps[i].From->ID==stID)){
			cnts[ps[i].Dest->ID] = cnts[ps[i].Dest->ID] + 1; 
			// Increment element which store planets who passenger want
		}
	}
	for(i=0; i<5; i=i+1){ //find maximum
		if(cnts[i]>max){
			max = cnts[i];
			ID = i;
		}
	}
	
	// If this station have no passengers
	if(max<1){
		int cntP = (stID + 1) % 5;
		do{
			max = stations[cntP].PasCnt;
			ID = stations[cntP].ID;
			if(ID == stID){ // exception: no more passengers
				return -1;
			}
			cntP = (cntP + 1) % 5;
		}while(max<1);
	}
	
	return ID;
}

void disembark(Ship *ship){
	int i;
	for(i=0; i<ps_cnt; i=i+1){
		if((ps[i].State == FLY) & (ps[i].Transport == ship)){
			ps[i].State = LANDING;
			ship->FreeSpace++;
		}
	}
}

Station* ship_arrival(Ship *ship){
	// Find destination for max passengers
	int wantedArrive = find_max_dest(ship->Dest->ID); 
	if(wantedArrive==-1){ // if no more passengers
		disembark(ship);
		return ship->Dest;
//		wantedArrive = rand_exclude(5, ship->Dest->ID, ship->ID);
	}
	
	// Notify station about ship and next Dest
	ship->Dest->ShipInPortID = ship->ID;
	ship->Dest->ArriveTo = wantedArrive;
	
	// Change position of passengers in ship and release sem
	disembark(ship);	
	
	// Wait for arriving passengers
	Sleep(STOP_DELAY);	
	
	// Notify station about ship departure
	ship->Dest->ArriveTo = -1; // no station
	ship->Dest->ShipInPortID = -1;
	
	// Change ship next dest and return it
	return ship_nextDest(ship, wantedArrive);
	
//	ship->Dest = &stations[wantedArrive];
}

void* ship_modeling(void *arg){
	Ship *ship_m = (Ship*) arg;
	Station *dest;
	short flagMove=1;
	int next = rand_exclude(5, ship_m->Dest->ID, ship_m->ID);
	dest = ship_nextDest(ship_m, next);
		
	// Cycle of flying
	while(flagMove>0){
		double dx = cos(ship_m->Direction);
		double dy = sin(ship_m->Direction);
		
		// Flying while ship not in planet radius
		while( !(	((dest->X+PLANET_RADIUS > ship_m->X)&(dest->X-PLANET_RADIUS < ship_m->X)) &
					((dest->Y+PLANET_RADIUS > ship_m->Y)&(dest->Y-PLANET_RADIUS < ship_m->Y)) ))
		{
			ship_m->X = ship_m->X+dx;
			ship_m->Y = ship_m->Y+dy;
			Sleep(SHIP_SPEED);
		}
		
		// Wait in the planet's orbit
		pthread_mutex_lock(&dest->Port_mut);
			// Now ship in station
			toConsoleSprintf("Ship <%s> arrived to station <%s>", 
						ship_m->Name,ship_m->Dest->Name);
			ship_m->X = ship_m->Dest->X;
			ship_m->Y = ship_m->Dest->Y;
						
			// Change inner passengers position to current station			
			if(ship_arrival(ship_m) == dest){
				flagMove = 0;
			}
		// Leave port	
		pthread_mutex_unlock(&dest->Port_mut);
		toConsoleSprintf("Ship <%s> leaves station <%s>", 
						ship_m->Name,ship_m->Dest->Name);
		dest = ship_m->Dest;
	}
	
	toConsoleSprintf("Ship <%s> stay at station and doesnt see any passenger. Stop fly.", 
						ship_m->Name);
	
	return 0;
}

// ******* Threads *******

void init_data(){
	int j=0;
	int cnt=1;
	int tempID;
	
	// Stations init	
	stations[0] = (Station){0, 122,465, {'A', 'l', 't', 'a', 'i', 'r'}, 		NULL, -1, -1, 0};
	stations[1] = (Station){1, 386,465, {'C','a','n','o','p','u','s'}, 			NULL, -1, -1, 0};
	stations[2] = (Station){2, 465,215, {'C','a','p','e','l','l','a'}, 			NULL, -1, -1, 0};
	stations[3] = (Station){3, 40,215,  {'N','e','w','-','T','e','r','r','a'}, 	NULL, -1, -1, 0};
	stations[4] = (Station){4, 255,60,  {'E','a','r','t','h'}, 					NULL, -1, -1, 0};
	
	// Ships init
	ships[0] = (Ship){0, {'I', 'N', 'K'}, &stations[0], 	0.7, 150, 200, 5, NULL};
	ships[1] = (Ship){1, {'D', 'E', 'F'}, &stations[1], 	-1.3,200, 100, 5, NULL};
	ships[2] = (Ship){2, {'A', 'R', 'K'}, &stations[2], 	0, 	  32, 300, 5, NULL};

	// Passenger generating
	ps_cnt = rand() % 5 + 14; //Make around 16 ps's
	ps = (Passenger*) malloc(sizeof(Passenger) * ps_cnt);
	
	for(j=0;j<ps_cnt;j=j+1){
//		ps[j] = (Passenger){cnt, rand()%5, -1, WAITING, 0};
		ps[j] = (Passenger){cnt, WAITING, 0, NULL, NULL, NULL};
		
		tempID = rand_exclude(5, -1, cnt); // Generate position (exclude -1)
		ps[j].From = &stations[tempID];
		
		tempID = rand_exclude(5, tempID, cnt); // Exclude FROM
		ps[j].Dest = &stations[tempID];
		
//		ps[j].Dest.PasCnt = stations[ps[j].Position].PasCnt + 1;
//		ps[j].Dest->PasCnt++;
		cnt = cnt+1;
	}
}

void start_threads(){
	int j;
	
	// Init station port mutex
	for(j=0;j<5;j=j+1){
		pthread_mutex_init(&stations[j].Port_mut,NULL);
	}	
	
	// Init passenger threads
	threads_ps = (pthread_t*) malloc(sizeof(pthread_t) * ps_cnt);
	for(j=0;j<ps_cnt; j=j+1){
		pthread_create(&threads_ps[j], NULL, passenger_modeling, (void*) &ps[j]);
	}
	
	// Init ships threads and queue mutex
	for(j=0;j<3;j=j+1)	{		
		pthread_mutex_init(&ships[j].Queue_mut,NULL);
		pthread_create(&threads_ships[j], NULL, ship_modeling, (void*) &ships[j]);	 
	}	
}

void stop_threads(){
	int j;
	
	for(j=0;j<5;j=j+1){
		pthread_mutex_destroy(&stations[j].Port_mut);
	}	
	for(j=0;j<3;j=j+1)	{		
		pthread_mutex_destroy(&ships[j].Queue_mut);
		pthread_exit(&threads_ships[j]);	 
	}
	for(j=0;j<ps_cnt; j=j+1){
		pthread_exit(&threads_ps[j]);
	}
}

// ******* Drawning *******

void DrawPlanets(HDC hdc){
	// Solid pen for planet
	HPEN pen = CreatePen(PS_SOLID, 2, 0);
	SelectObject(hdc, pen);
	char temp[16];
	// Draw planets and they names
	int i=0;
	for(; i<5; i=i+1){
		memset(temp, 0, sizeof(temp));
		int x = stations[i].X;
		int y = stations[i].Y;
		Ellipse(hdc, x-PLANET_RADIUS, y-PLANET_RADIUS, x+PLANET_RADIUS, y+PLANET_RADIUS);
		sprintf(temp, "%s\n(%d)", stations[i].Name, stations[i].PasCnt);
		TextOut(hdc, x+PLANET_RADIUS/3, y-PLANET_RADIUS*1.7, temp,15);
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

void DrawShips (HDC hdc){
	HBRUSH brush = CreateSolidBrush(0);
	SelectObject(hdc, brush);
	char mes[9];
	short dx = 10;
	short dy = 8;
	int i=0;
	for(;i<3;i=i+1)
	{		
		int x = (int)ships[i].X;
		int y = (int)ships[i].Y;
		POINT triangle[] = {{ x, y-dy},  { x-dx, y+dy}, { x+dx, y+dy}};
		Polygon(hdc, triangle, 3);
		sprintf(mes, "%s (%d/5)", ships[i].Name, 5-ships[i].FreeSpace);
		TextOut(hdc, x+10, y+10, mes, 9);
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
//	DrawPassengerList(hMemDC);
	
	BitBlt(hdc, 0, 0, width, height, hMemDC, 0, 0, SRCCOPY);
	DeleteObject(hMemBitmap);
	DeleteDC(hMemDC);
}

// ******* WinApi *******

void toConsoleSprintf (char *fmt, ...) {
	char buf[128];
    va_list va;
    va_start (va, fmt);
    vsprintf (buf, fmt, va);
    va_end (va);
    SendMessage(consoleBox, LB_ADDSTRING, 1, (LPARAM)buf);
}

void LVPassengers_InitColumns(HWND hwndLV){ 
   	LVCOLUMN lvc;
	int i;
	char cTitles[6][12];
	
	strcpy(cTitles[0], "ID");
	strcpy(cTitles[1], "State");
	strcpy(cTitles[2], "From");
	strcpy(cTitles[3], "Position");
	strcpy(cTitles[4], "Destination");
	strcpy(cTitles[5], "Travel count");
	
	for(i=0;i<6;i=i+1){
		memset(&lvc, 0, sizeof(lvc));
		lvc.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM;
		lvc.fmt = LVCFMT_LEFT;
		lvc.cx = 65;
		lvc.pszText = cTitles[i];
		lvc.iSubItem = i;
		ListView_InsertColumn(hwndLV, i, & lvc);
	}
	ListView_SetColumnWidth(hwndLV, 0, 30); //Set id col width 30
} 

void LVPassengers_LoadItems(HWND hwndLV){
	int i, gid;
	char temp[3];
	LVITEM lvItem;
	
	ListView_DeleteAllItems(hwndLV);
	
	for(i=0; i<ps_cnt; i=i+1){

		memset(&lvItem, 0, sizeof(lvItem));
		memset(&temp, 0, sizeof(temp));
		
		sprintf(temp, "%d", ps[i].ID);
		lvItem.pszText   = temp;
	    lvItem.mask      = LVIF_TEXT | LVIF_STATE;
	    lvItem.stateMask = 0;
	    lvItem.iSubItem  = 0;
	    lvItem.state     = 0;
		ListView_InsertItem(hwndLV, &lvItem);
		
		// Set state
		lvItem.iSubItem = 1;
		switch(ps[i].State){
			case LANDING: lvItem.pszText = "Landing"; break;
			case FLY: lvItem.pszText = "Flying"; break;
			case ARRIVE: lvItem.pszText = "Arrive"; break;
			case WAITING: lvItem.pszText = "Waiting"; break;
			case END_TRAVEL: lvItem.pszText = "Stop"; break;
		}
		ListView_SetItem(hwndLV, &lvItem);		
		
		// Set From
		lvItem.iSubItem = 2;
		lvItem.pszText = ps[i].From->Name;
		ListView_SetItem(hwndLV, &lvItem);		
		
		// Set Position
		if(ps[i].Transport != NULL){
			lvItem.iSubItem = 3;
			lvItem.pszText = ps[i].Transport->Name;
			ListView_SetItem(hwndLV, &lvItem);		
		}
		
		// Set Destination
//		if(ps[i].Dest != NULL){
		lvItem.iSubItem = 4;
		lvItem.pszText = ps[i].Dest->Name;
		ListView_SetItem(hwndLV, &lvItem);	
//		}
		
		// Set TrvCnt
		lvItem.iSubItem = 5;
		memset(&temp, 0, sizeof(temp));
		sprintf(temp, "%d", ps[i].TrvCnt);
		lvItem.pszText = temp;
		ListView_SetItem(hwndLV, &lvItem);	
	}
}

LRESULT OnCommand(HWND hwnd, int id, HWND hwndCtl, UINT codeNotify){
	switch(id){
		case 5: {break;}
	}
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT Message, WPARAM wParam, LPARAM lParam) {
	HDC hdc;
	PAINTSTRUCT paintStr;
	switch(Message) {
		case WM_DESTROY: {
			stop_threads();
			PostQuitMessage(0);
			break;
		}
		case WM_TIMER: {
			switch(wParam){
				case TM_DRAW:{
					InvalidateRect(hwnd, NULL, FALSE);
					break;
				}
				case TM_PASSENGER:{
					LVPassengers_LoadItems(listViewPassengers);
					break;
				}
			}
			break;
		}
		case WM_PAINT: {
			hdc = BeginPaint(hwnd, &paintStr);
			DrawComponents(hdc, paintStr.rcPaint);
			EndPaint(hwnd, &paintStr);
			break;
		}
		case WM_MOVE:{
			InvalidateRect(hwnd, NULL, FALSE);
			break;
		}
		case WM_ERASEBKGND: {
			return 1;
		}
		
		default: return DefWindowProc(hwnd, Message, wParam, lParam);
	}
//	HANDLE_MSG(hwnd, WM_COMMAND, OnCommand);
	return 0;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
	WNDCLASSEX wc; /* A properties struct of our window */
	
	init_data();
	
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
	 startWindow = CreateWindow("frame","Galactic democratic transport modeling program",
	 		WS_VISIBLE|WS_OVERLAPPED | WS_SYSMENU | WS_CLIPCHILDREN |
			WS_TILEDWINDOW | WS_VISIBLE | LBS_STANDARD,
			100, 900, 800, 360,
			hwnd,NULL,hInstance,NULL);

	//main window
	hwnd = CreateWindowEx(WS_EX_CLIENTEDGE,"WindowClass","Galactic democratic transport modeling program", 
		WS_VISIBLE|WS_OVERLAPPED | WS_SYSMENU | WS_CLIPCHILDREN ,
		CW_USEDEFAULT, /* x */
		CW_USEDEFAULT, /* y */
		1120, /* width */
		800, /* height */
		NULL,NULL,hInstance,NULL);

	consoleBox = CreateWindow(WC_LISTBOX, NULL,
   			WS_CHILD | WS_VISIBLE |  WS_BORDER | WS_VSCROLL | LBS_DISABLENOSCROLL | LBS_WANTKEYBOARDINPUT,
   			10, 530, 	500, 240,
   			hwnd, (HMENU) 2, hInstance, NULL);
   	
   	listViewPassengers = CreateWindow(WC_LISTVIEW, NULL,
		    WS_VISIBLE | WS_CHILD | LVS_REPORT | LVS_EDITLABELS,
		    700, 10,    1120-700-10, 800-20,
		    hwnd, (HMENU)3, hInstance,   NULL);
	LVPassengers_InitColumns(listViewPassengers);
	LVPassengers_LoadItems(listViewPassengers);

	SetTimer(hwnd, TM_DRAW, 		10, (TIMERPROC)NULL); // 10ms timer for drawning
	SetTimer(hwnd, TM_PASSENGER, 	1000, (TIMERPROC)NULL); // 10ms timer for upd passengers
	start_threads();

	while(GetMessage(&msg, NULL, 0, 0) > 0) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	return msg.wParam;
}
