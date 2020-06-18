#define _CRT_SECURE_NO_WARNINGS
#include "framework.h"
#include <CommDlg.h>
#include <math.h>
#include "resource.h"
#include <stdio.h>
//#include "openFileDialog-RC.h"
#include <string.h>
#include "Banner-T3.h"

//-------------------------------------------------------------------------------------------------
// Macro Definitions
#define MAX_LOADSTRING 100
#define RADIUS 5

#define countof(strucName) (sizeof(strucName) / sizeof(strucName[0]))

#define VERTEX_NOSELECTED       -2
#define VERTEX_INTEXT_1			-3
#define VERTEX_INTEXT_2			-4
#define VERTEX_INTEXT_3			-5

#define VERTEX_ADJUSTWIDTH      0
#define VERTEX_ADJUSTHEIGHT     1
#define VERTEX_ADJUSTDIAGONAL   2
#define VERTEX_IMAGE_LENGTH     3

#define STD_POINT		30
#define GRADIENT_WIDTH  1440
#define GRADIENT_HEIGHT 720
#define CL 5                                                        // 이미지 테두리 

#define PREV_LEFT	12
#define PREV_TOP	50
#define PREV_RIGHT  533
#define PREV_BOTTOM 780

//작업 데이터
#define PAPERXQTY   6
#define PAPERYQTY   3

#define PAPERXSIZE  2100                                            // A4 가로 mm
#define PAPERYSIZE  2970                                            // A4 세로 mm

#define ZOOMBASE    1000

typedef const unsigned char* LPCBYTE;
typedef const int* LPCINT;
typedef short* LPINT16;
typedef const short* LPCINT16;

#define LoInt16(L)      *(LPINT16)&L            //ARM에서는 선별적으로 사용할 것
#define HiInt16(L)      *((LPINT16)&L+1)        //      "
#define LoWord(DW)      *(LPWORD)&DW            //      "
#define HiWord(DW)      *((LPWORD)&DW+1)        //      "

//-------------------------------------------------------------------------------------------------
// Global Variables
HINSTANCE hInst;                                // 현재 인스턴스입니다.
WCHAR szTitle[MAX_LOADSTRING];                  // 제목 표시줄 텍스트입니다.
WCHAR szWindowClass[MAX_LOADSTRING];            // 기본 창 클래스 이름입니다.

static int g_zoomVal = 100;
static int g_panningX, g_panningY;              // Work영역좌표
static int g_imgSzX, g_imgSzY;                  // 원본이미지 px -> mm로 변환한 값
static int g_textHeight = 300;

static HWND g_hButtonOpenFileDialog;			// 파일열기 대화상자를 실행하기 위한 버튼의 핸들
static HWND g_hEditFileToBeOpened;				// 파일의 경로와 이름을 가져오는 에디트 컨트롤의 핸들
static char g_imgRoute[256];					// 이미지 경로 ex)"C:\project\Banner\banner\girl.bmp"

static int		g_textSz[3];					// 사용자가 콤보박스에서 선택한 글자 크기(pt)
static char		g_textFont[100];				// 사용자가 콤보박스에서 선택한 글꼴
static char*    g_textFontArr[3];				// 사용자가 선택한 글꼴을 모아놓은 배열
static char		g_textStr[1024];				// 사용자가 입력한 텍스트
static char*    g_textStrArr[3];			    // 사용자가 입력한 텍스트를 모아놓은 배열
static int		g_textCnt = 0;					// 텍스트 개수
static RGBQUAD	g_textColor[3];					// 사용자가 선택한 글자 색상

//작업중인 이미지 관련
static HBITMAP	g_hImageLoaded;					// 로딩된 HBITMAP
static RECT     g_imgRect;                      // 이미지 좌표

//작업중인 텍스트 관련
static RECT g_textRect[3];						// 텍스트 외곽 좌표
static int  g_prevTextSz;						// 미리보기에서 텍스트 크기

//프린터 구조체
struct PRINTERINFO
{
    int xRes;									// A4 기준 pixel
    int yRes;
    int xSize;									// A4 기준 mm
    int ySize;
    int xDpi;
    int yDpi;

    int pW;
    int pH;
    int pOL;
    int xResMtp;							    // a4 너비 사이즈 배수
    int yResMtp;								// a4 높이 사이즈 배수
    int ng_ImageWidth;
    int ng_ImageHeight;
    int currPage = 1;							// 현재 페이지
    int endPage;								// 마지막 페이지
};
PRINTERINFO pi;									// Printer Information

//-----------------------------------------------------------------------------
//      PrinterDC를 호출합니다
//-----------------------------------------------------------------------------
HDC WINAPI GetPrinterDC(HWND Hwnd)
{
    HDC hdc;
    PRINTDLG pd;

    memset(&pd, 0, sizeof(PRINTDLG));
    pd.lStructSize = sizeof(pd);
    pd.hwndOwner = Hwnd;
    pd.Flags = PD_RETURNDC; // PD_ALLPAGES | PD_RETURNDC | PD_NOSELECTION | PD_ENABLEPRINTTEMPLATE | PD_ENABLEPRINTHOOK

    // Retrieves the printer DC
    PrintDlg(&pd);
    hdc = pd.hDC;
    return hdc;
}



//-----------------------------------------------------------------------------
//      선을 그림
//-----------------------------------------------------------------------------
void WINAPI DrawLine(HDC hDC, int X1, int Y1, int X2, int Y2)
{
    MoveToEx(hDC, X1, Y1, NULL);
    LineTo(hDC, X2, Y2);
}



//-----------------------------------------------------------------------------
//      원을 그림
//-----------------------------------------------------------------------------
void WINAPI DrawCircle(HDC hDC, int X, int Y, int Rad)
{
    Ellipse(hDC, X - Rad, Y - Rad, X + Rad, Y + Rad);
}



//-----------------------------------------------------------------------------
//      WorkSpace 좌표와 Device 좌표 사이를 서로 변환
//-----------------------------------------------------------------------------
int W2D(int R)  { return MulDiv(R, g_zoomVal, ZOOMBASE); }                                  // 크기 변환하는 함수
int W2DX(int X) { return MulDiv(X, g_zoomVal, ZOOMBASE); }
int W2DY(int Y) { return MulDiv(Y, g_zoomVal, ZOOMBASE); }
int W2DXImg(int X) { return MulDiv(X - g_panningX, g_zoomVal, ZOOMBASE); }                  // 이미지 부분에 사용(StretchBlt)
int W2DYImg(int Y) { return MulDiv(Y - g_panningY, g_zoomVal, ZOOMBASE); }                  // 이미지 부분에 사용(StretchBlt)

int D2W(int R)  { return MulDiv(R, ZOOMBASE, g_zoomVal); }
int D2WX(int X) { return MulDiv(X, ZOOMBASE, g_zoomVal) + g_panningX; }
int D2WY(int Y) { return MulDiv(Y, ZOOMBASE, g_zoomVal) + g_panningY; }



//-----------------------------------------------------------------------------
//      메인 윈도우의 이미지 테두리를 클릭했을 때, 경계선을 그립니다.
//-----------------------------------------------------------------------------
void WINAPI DrawImgBoundaryLine(HDC hdc)
{
    SelectObject(hdc, GetStockObject(NULL_BRUSH));  //면을 칠하지 않도록 함
    Rectangle(hdc, W2DXImg(0), W2DYImg(0), W2DXImg(g_imgSzX), W2DYImg(g_imgSzY));
}



//-----------------------------------------------------------------------------
//      Bmp파일을 HBITMAP로 로딩하여 리턴
//-----------------------------------------------------------------------------
void WINAPI LoadBmpImage(void)
{
    BITMAP bm;
    //int tx, ty;       // 원본 이미지(px)를 mm로 변환한 값을 잠시 넣어놓음

    if ((g_hImageLoaded = (HBITMAP)LoadImage(NULL, g_imgRoute, IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION)) != NULL)
    {
        GetObject(g_hImageLoaded, sizeof(BITMAP), &bm);

        //tx = (int)((bm.bmWidth / 25.4) * 120);       // 120은 이미지의 dpi를 의미한다(일단 정적으로 해놓음)
        //ty = (int)((bm.bmHeight / 25.4) * 120);
        g_imgSzX = (int)((bm.bmWidth / 25.4) * 120);       // 120은 이미지의 dpi를 의미한다(일단 정적으로 해놓음)
        g_imgSzY = (int)((bm.bmHeight / 25.4) * 120);
    }
}



//-----------------------------------------------------------------------------
//       원본이미지를 크기조절 후 화면에 출력
//-----------------------------------------------------------------------------
void WINAPI DrawStretchBitmap(HDC hDC, HBITMAP hBtm, BITMAP bm, int X1, int Y1, int X2, int Y2)
{
    HDC     hMemDC;
    HBITMAP hBtmOld;
    hMemDC = CreateCompatibleDC(hDC);
    hBtmOld = (HBITMAP)SelectObject(hMemDC, hBtm);                                          // hBtm가 선택되기 전의 핸들을 저장해 둔다
    StretchBlt(hDC, X1, Y1, X2, Y2, hMemDC, 0, 0, bm.bmWidth, bm.bmHeight, SRCCOPY);
    printf("원본(px) 가로: %d, 세로: %d -> ", bm.bmWidth, bm.bmHeight);
    printf("수정(mm) 가로: %d, 세로: %d\n", X2, Y2);
    SelectObject(hMemDC, hBtmOld);                                                          // hImage 선택을 해제하기 위해 hBtmOld을 선택한다
    DeleteDC(hMemDC);
}



//-----------------------------------------------------------------------------
//      주어진 이름의 글꼴 정보를 얻습니다
//-----------------------------------------------------------------------------
static int CALLBACK GetFontInfoEnumProc(const LOGFONT* LF, const TEXTMETRIC* TM, DWORD Type, LPARAM lPrm)
{
    *(LOGFONT*)lPrm = *LF;
    return TRUE;
}

BOOL WINAPI GetFontInfo(LPCSTR FontFace, LOGFONT* LF)
{
    int Rslt;
    HDC hDC;

    hDC = GetDC(NULL);
    Rslt = EnumFonts(hDC, FontFace, GetFontInfoEnumProc, (LPARAM)LF);
    ReleaseDC(NULL, hDC);
    return Rslt;
}



//-----------------------------------------------------------------------------
//      글꼴을 만듭니다
//-----------------------------------------------------------------------------
HFONT WINAPI MyCreateFont(int Height, BOOL BoldFg, BOOL ItalicFg, LPCSTR FontName)
{
    LOGFONT LF;

    GetFontInfo(FontName, &LF);
    return CreateFont(Height,                   //높이
        0,                                      //MulDiv(Size, WidthRate, 200); //폭
        0,                                      //0.1도 단위의 글자 방향
        0,                                      //방향으로 설명되어 있으나 먹통임
        BoldFg == 0 ? FW_DONTCARE : FW_BOLD,    //글자 굵기
        ItalicFg,                               //1:이탤릭
        0,                                      //1:밑줄문자
        0,                                      //1:중앙통과선
        LF.lfCharSet,
        OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY,
        FF_DONTCARE | DEFAULT_PITCH,
        FontName);
}



//-----------------------------------------------------------------------------
//		글꼴, 색상는 여기서 모두 작업합니다
//-----------------------------------------------------------------------------
void WINAPI SaveTextInfo(HWND hDlg)
{
    GetDlgItemText(hDlg, IDSTRING, g_textStr, sizeof(g_textStr));
    GetDlgItemText(hDlg, IDFONT, g_textFont, sizeof(g_textFont));
    g_textSz[g_textCnt] = GetDlgItemInt(hDlg, IDSIZE, NULL, FALSE);

    if (g_textStrArr[g_textCnt] == NULL)
    {
        g_textStrArr[g_textCnt] = (char*)malloc(sizeof(char) * sizeof(g_textStr));
        g_textFontArr[g_textCnt] = (char*)malloc(sizeof(char) * sizeof(g_textFont));
    }

    if (g_textCnt != 3)
    {
        strcpy(g_textStrArr[g_textCnt], g_textStr);
        strcpy(g_textFontArr[g_textCnt], g_textFont);
        g_textCnt++;
    }
}



//-----------------------------------------------------------------------------
//      16진수 -> RGB로 변환합니다
//-----------------------------------------------------------------------------
RGBQUAD colorConverter(int hexValue)
{
    RGBQUAD rgbColor;
    rgbColor.rgbRed = ((hexValue >> 16) & 0xFF);	// Extract the RR byte
    rgbColor.rgbGreen = ((hexValue >> 8) & 0xFF);	// Extract the GG byte
    rgbColor.rgbBlue = ((hexValue) & 0xFF);			// Extract the BB byte

    return rgbColor;
}



//-----------------------------------------------------------------------------
//      글자 색상을 처리합니다.
//-----------------------------------------------------------------------------
void WINAPI ChoiceTextColor(HWND hDlg)
{
    COLORREF textColor; // 사용자가 선택한 색상
    COLORREF crTemp[16];
    CHOOSECOLOR col;

    memset(&col, 0, sizeof(CHOOSECOLOR));
    col.lStructSize = sizeof(CHOOSECOLOR);
    col.hwndOwner = hDlg;

    col.lpCustColors = crTemp;
    col.Flags = 0;

    if (ChooseColor(&col) != 0)
    {
        textColor = col.rgbResult;
        g_textColor[g_textCnt] = colorConverter(textColor);
        //printf("Color: %lu\n", textColor); // 사용자가 선택한 색상 십진법으로 출력      
        //printf("RGB(%u, %u, %u)\n", g_textColor.rgbRed, g_textColor.rgbGreen, g_textColor.rgbBlue);
    }
}



//-----------------------------------------------------------------------------
//      프린터를 선택하면 해당 프린터의 정보를 읽습니다.
//-----------------------------------------------------------------------------
HDC WINAPI ReadPrinterInfo(HWND hWnd)
{
    HDC prn = GetPrinterDC(hWnd);

    pi.xRes = GetDeviceCaps(prn, HORZRES);
    pi.yRes = GetDeviceCaps(prn, VERTRES);
    pi.xSize = GetDeviceCaps(prn, HORZSIZE);
    pi.ySize = GetDeviceCaps(prn, VERTSIZE);
    pi.xDpi = GetDeviceCaps(prn, LOGPIXELSX);
    pi.yDpi = GetDeviceCaps(prn, LOGPIXELSY);

    printf("X크기(픽셀)=%d, Y크기(픽셀)=%d\n", pi.xRes, pi.yRes);
    printf("X크기(mm)=%d, Y크기(mm)=%d\n", pi.xSize, pi.ySize);
    printf("X DPI=%d, Y DPI=%d\n\n", pi.xDpi, pi.yDpi);

    return prn;
}



//-----------------------------------------------------------------------------
//      글꼴, 글자크기 콤보박스는 여기서 작업합니다
//-----------------------------------------------------------------------------
void WINAPI CreateComboBox(HWND hDlg)
{
    HWND fontDlg, szDlg;
    BOOL fontInit, szInit;

    char fonts[][100] = { "궁서", "굴림", "돋움", "나눔고딕", "HY견고딕" };
    char sz[][100] = { "10", "11", "12", "13", "14", "16", "18", "20", "24", "28", "32", "36", "40", "48", "72", "96", "120" };

    fontInit = SetDlgItemText(hDlg, IDFONT, fonts[0]);
    szInit = SetDlgItemText(hDlg, IDSIZE, sz[0]);

    fontDlg = GetDlgItem(hDlg, IDFONT);
    szDlg = GetDlgItem(hDlg, IDSIZE);

    for (int i = 0; i < 5; i++)
        SendMessage(fontDlg, CB_ADDSTRING, 0, (LPARAM)fonts[i]);

    for (int i = 0; i < 17; i++)
        SendMessage(szDlg, CB_ADDSTRING, 0, (LPARAM)sz[i]);

    SetFocus(GetDlgItem(hDlg, IDSTRING)); // 적용이 왜 안되지..
}



//-----------------------------------------------------------------------------
//		윈도우 영역에서 글꼴, 색상, 크기는 여기서 모두 작업합니다
//-----------------------------------------------------------------------------
void WINAPI DrawTextAll(HWND hDlg)
{
    HDC hdc = GetDC(hDlg);
    POINT pt;
    GetCursorPos(&pt);
    HFONT hFont, oldFont;

    for (int i = 0; i < g_textCnt; i++)
    {
        //hFontOld = (HFONT)SelectObject(hDC, MyCreateFont(W2D(g_textSz[0]), FALSE, FALSE, "궁서")); // {2: Bold, 3: Italic}
        //SetRect(&R, W2DX(1000), W2DY(3000), W2DX(3000), W2DY(3500));
        //DrawText(hDC, "TEXT 테스트중", -1, &R, DT_VCENTER | DT_WORDBREAK);
        //DeleteObject(SelectObject(hDC, hFontOld));

        // 잠시 첫 번째 인자는 W2D(g_textSz[i]) 였음
        hFont = MyCreateFont(g_textSz[i], FALSE, FALSE, TEXT(g_textFontArr[i])); // {2: Bold, 3: Italic}
        oldFont = (HFONT)SelectObject(hdc, hFont);

        SetTextColor(hdc, RGB(g_textColor[i].rgbBlue, g_textColor[i].rgbGreen, g_textColor[i].rgbRed));
        SetBkMode(hdc, TRANSPARENT);

        //SelectClipRgn(hdc, NULL); // 우선 전체영역을 출력하기 위해 NULL처리
        //IntersectClipRect(hdc, STD_POINT, STD_POINT, g_ImageWidth + STD_POINT, g_ImageHeight + STD_POINT); // 미리보기 영역에만 이미지를 출력하기 위함

        DrawTextEx(hdc, g_textStrArr[i], -1, &g_textRect[i], DT_CALCRECT, NULL);					//실제 문자열은 찍지 않고, 그려질 area만 측정되어 rt에 저장된다.
        DrawTextEx(hdc, g_textStrArr[i], -1, &g_textRect[i], DT_VCENTER | DT_WORDBREAK, NULL);		//실제 문자열을 찍는 문장. rt에 출력한다.

        SelectObject(hdc, oldFont);
        DeleteObject(hFont);
    }
    // 처음엔 PAINT 메세지가 발생하여 null, null, null로 찍힘
    //printf("g_textStrArr[0]: %s\n", g_textStrArr[0]);
    //printf("g_textStrArr[1]: %s\n", g_textStrArr[1]);
    //printf("g_textStrArr[2]: %s\n", g_textStrArr[2]);
    //printf("g_textCnt: %d\n", g_textCnt);

    ReleaseDC(hDlg, hdc);
}



//-----------------------------------------------------------------------------
//      모든 화면 그리는 동작
//-----------------------------------------------------------------------------
void WINAPI DrawAll(HWND hWnd, HDC hDC)
{
    BITMAP  bm;
    int X, Y, px, py;

    for (Y = 0; Y < PAPERYQTY; Y++)
    {
        for (X = 0; X < PAPERXQTY; X++)
        {
            px = X * PAPERXSIZE;
            py = Y * PAPERYSIZE;
            Rectangle(hDC, W2DX(px), W2DY(py), W2DX(px + PAPERXSIZE), W2DY(py + PAPERYSIZE));
        }
    }
    GetObject(g_hImageLoaded, sizeof(BITMAP), &bm);

    g_imgSzX = (int)((bm.bmWidth / 25.4) * 120);       // 120은 이미지의 dpi를 의미한다(일단 정적으로 해놓음)
    g_imgSzY = (int)((bm.bmHeight / 25.4) * 120);

    DrawStretchBitmap(hDC, g_hImageLoaded, bm, W2DX(0), W2DY(0), W2DXImg(g_imgSzX), W2DYImg(g_imgSzY));
    DrawTextAll(hWnd);
}



//-----------------------------------------------------------------------------
//      메인 윈도우의 이미지 테두리를 클릭했을 때, 꼭짓점(원)을 그립니다.
//-----------------------------------------------------------------------------
void WINAPI DrawImgVertex(HDC hdc)
{
    DrawCircle(hdc, W2DXImg(g_imgSzX),         (W2DYImg(g_imgSzY) / 2),   RADIUS);		// 우측 모서리 중앙
    DrawCircle(hdc, (W2DXImg(g_imgSzX) / 2),   W2DYImg(g_imgSzY),         RADIUS);		// 아래 모서리 중앙
    DrawCircle(hdc, W2DXImg(g_imgSzX),         W2DYImg(g_imgSzY),         RADIUS);		// 우측 하단 꼭짓점 
}



//-----------------------------------------------------------------------------
//      크기 조정중인 안내선을 그림
//-----------------------------------------------------------------------------
void WINAPI DrawSizeInfoLine(HWND hWnd)
{
    HDC hdc = GetDC(hWnd);

    SetROP2(hdc, R2_XORPEN);   // GDI함수가 화면에 출력을 내보낼 때 화면에 이미 출력되어 있는 그림과 새로 그려지는 그림과의 관계를 정의하는 함수
    SelectObject(hdc, GetStockObject(WHITE_PEN));
    DrawImgBoundaryLine(hdc);  // 이미지 경계선
    DrawTextAll(hWnd);
    DrawImgVertex(hdc);		   // 이미지 꼭짓점 
    InvalidateRect(hWnd, NULL, FALSE); // 적용하면 텍스트 잔상처리가 해결되지만 반짝거리고 안하면 텍스트 잔상처리가 해결안됨
    ReleaseDC(hWnd, hdc);
}



//-----------------------------------------------------------------------------
//      이미지 크기를 조정할 위치정보를 리턴함
//-----------------------------------------------------------------------------
int WINAPI GetDragingMode(HWND hWnd, POINT P)
{
    HDC hdc = GetDC(hWnd);
    RECT R = { 0, 0, 0, 0, };
    int  DragingMode = VERTEX_NOSELECTED;

    SetRect(&R, W2DX(g_imgSzX) - CL, W2DY(g_imgSzY) - CL, W2DX(g_imgSzX) + CL, W2DY(g_imgSzY) + CL);
    if (PtInRect(&R, P))
        DragingMode = VERTEX_ADJUSTDIAGONAL;
    else
    {
        SetRect(&R, W2DX(g_imgSzX) - CL, W2DY(0), W2DX(g_imgSzX) + CL, W2DY(g_imgSzY) - CL);
        if (PtInRect(&R, P))
            DragingMode = VERTEX_ADJUSTWIDTH;
        else
        {
            SetRect(&R, W2DX(0), W2DY(g_imgSzY) - CL, W2DX(g_imgSzX) - CL, W2DY(g_imgSzY) + CL);
            if (PtInRect(&R, P))
                DragingMode = VERTEX_ADJUSTHEIGHT;
            else
            {
                if (PtInRect(&g_textRect[0], P))
                    DragingMode = VERTEX_INTEXT_1;
                else if (PtInRect(&g_textRect[1], P))
                    DragingMode = VERTEX_INTEXT_2;
                else if (PtInRect(&g_textRect[2], P))
                    DragingMode = VERTEX_INTEXT_3;
            }
        }
    }
    ReleaseDC(hWnd, hdc);

    return DragingMode;
}



//-----------------------------------------------------------------------------
//      드래깅 모드를 처리합니다.
//-----------------------------------------------------------------------------
void WINAPI HandleDragingMode(HWND hWnd, LPARAM lParam, int DragingMode)
{
    int status = 0; // 몇 번째 텍스트를 클릭했는지 판단하기 위함 (0 ~ 2)

    if (DragingMode == VERTEX_NOSELECTED)
        return;

    DrawSizeInfoLine(hWnd);         //기존 그려진 안내선을 지움(잔상처리)
    if (DragingMode == VERTEX_ADJUSTWIDTH || DragingMode == VERTEX_ADJUSTDIAGONAL)
    {
        g_imgSzX = LoInt16(lParam);
        //g_imgSzX = (int)((LoInt16(lParam) / 25.4) * 120);
        //printf("(int)((LoInt16(lParam) / 25.4) * 120): %d\n", (int)((LoInt16(lParam) / 25.4) * 120));
    }
        
    if (DragingMode == VERTEX_ADJUSTHEIGHT || DragingMode == VERTEX_ADJUSTDIAGONAL)
    {
        g_imgSzY = HiInt16(lParam);
        //g_imgSzY = (int)((HiInt16(lParam) / 25.4) * 120);
        //printf("(int)((HiInt16(lParam) / 25.4) * 120): %d\n", (int)((HiInt16(lParam) / 25.4) * 120));
    }
        
    if (DragingMode == VERTEX_INTEXT_1)
    {
        status = 0;
        g_textRect[status].left = LOWORD(lParam) - (g_textRect[status].right - g_textRect[status].left) / 2;
        g_textRect[status].top = HIWORD(lParam) - (g_textRect[status].bottom - g_textRect[status].top) / 2;
    }
    else if (DragingMode == VERTEX_INTEXT_2)
    {
        status = 1;
        g_textRect[status].left = LOWORD(lParam) - (g_textRect[status].right - g_textRect[status].left) / 2;
        g_textRect[status].top = HIWORD(lParam) - (g_textRect[status].bottom - g_textRect[status].top) / 2;
    }
    else if (DragingMode == VERTEX_INTEXT_3)
    {
        status = 2;
        g_textRect[status].left = LOWORD(lParam) - (g_textRect[status].right - g_textRect[status].left) / 2;
        g_textRect[status].top = HIWORD(lParam) - (g_textRect[status].bottom - g_textRect[status].top) / 2;
    }

    DrawSizeInfoLine(hWnd);
}



//-----------------------------------------------------------------------------
//      모든 화면 그리는 동작
//-----------------------------------------------------------------------------
void MousePanning(HWND hWnd, UINT Msg, WPARAM wPrm, LPARAM lPrm)
{
    static int DragingMode = VERTEX_NOSELECTED;
    static POINT oldP;

    switch (Msg)
    {
    case WM_LBUTTONDOWN:
        SetCapture(hWnd);
        oldP.x = LoInt16(lPrm);
        oldP.y = HiInt16(lPrm);

        printf("DragingMode  시작\n");
        if ((DragingMode = GetDragingMode(hWnd, oldP)) == VERTEX_NOSELECTED)
            break;
        printf("DragingMode: %d\n", DragingMode);
        DrawSizeInfoLine(hWnd);

        //printf("imgX: %d, imgY: %d\n", g_imgSzX, g_imgSzY);
        //printf("W2DX(imgX): %d, W2DY(imgY): %d\n", W2DX(g_imgSzX), W2DY(g_imgSzY));
        printf("P.x: %d, P.y: %d\n", oldP.x, oldP.y);
        break;

    case WM_MOUSEMOVE:
        if (GetCapture() == hWnd)
        {
            HandleDragingMode(hWnd, lPrm, DragingMode);
            g_panningX -= D2W(LoInt16(lPrm) - oldP.x);
            g_panningY -= D2W(HiInt16(lPrm) - oldP.y);
            InvalidateRect(hWnd, NULL, TRUE);
            oldP.x = LoInt16(lPrm);
            oldP.y = HiInt16(lPrm);
        }
        break;

    case WM_LBUTTONUP:
        ReleaseCapture();
    }
}



//----------------------------------------------------------------------------
//      파일을 열어 이미지를 선택합니다.
//----------------------------------------------------------------------------
BOOL WINAPI OpenImage(HWND hDlg, LPSTR Buff, int BuffSize, LPCSTR Title, LPCSTR Filter)
{
    OPENFILENAME ofn;
    BOOL rv; //return value

    ZeroMemory(&ofn, sizeof(OPENFILENAME));
    ofn.lStructSize = sizeof(OPENFILENAME);
    ofn.hwndOwner = hDlg;
    ofn.lpstrTitle = Title;
    ofn.lpstrFilter = Filter;
    ofn.lpstrFile = Buff;
    ofn.nMaxFile = BuffSize;

    // 파일열기 대화상자를 열고, 선택된 파일의 이름을 에디트 박스로 복사
    if ((rv = GetOpenFileName(&ofn)) != 0)
        SetWindowText(g_hEditFileToBeOpened, ofn.lpstrFile);

    return rv;
}



//-----------------------------------------------------------------------------
//      메인 윈도우 메세지 처리
//-----------------------------------------------------------------------------
void WINAPI KeyProc(HWND hWnd, int key)
{
    static int currZoom = 4;
    static const int zoomTable[] = { 2000, 1000, 500, 250, 125, 63 };

    switch (key)
    {
    case 'I':   //확대
        if (currZoom > 0)
        {
            g_zoomVal = zoomTable[--currZoom];
            InvalidateRect(hWnd, NULL, TRUE);
        }
        break;

    case 'O':   //축소
        if (currZoom < countof(zoomTable) - 1)
        {
            g_zoomVal = zoomTable[++currZoom];
            InvalidateRect(hWnd, NULL, TRUE);
        }
        break;

    case VK_LEFT:
        g_panningX += 100;
        InvalidateRect(hWnd, NULL, TRUE);
        break;

    case VK_RIGHT:
        g_panningX -= 100;
        InvalidateRect(hWnd, NULL, TRUE);
        break;

    case VK_UP:
        g_panningY += 100;
        InvalidateRect(hWnd, NULL, TRUE);
        break;

    case VK_DOWN:
        g_panningY -= 100;
        InvalidateRect(hWnd, NULL, TRUE);
        break;
    }
}



//-----------------------------------------------------------------------------
//      이미지 다이얼로그를 호출합니다
//-----------------------------------------------------------------------------
BOOL CALLBACK ImageDialogBoxProc(HWND hDlg, UINT iMessage, WPARAM wParam, LPARAM lParam)
{
    switch (iMessage)
    {
    case WM_INITDIALOG:
    {
        g_hButtonOpenFileDialog = GetDlgItem(hDlg, IDC_OPEN_FILE_BTN);
        g_hEditFileToBeOpened   = GetDlgItem(hDlg, IDC_IMG_ROUTE_EDIT);
        return TRUE;
    }

    case WM_PAINT:
        PAINTSTRUCT ps;
        BeginPaint(hDlg, &ps);
        EndPaint(hDlg, &ps);
        return TRUE;

    case WM_COMMAND:
    {
        switch (wParam)
        {
        case IDOK:
            GetDlgItemText(hDlg, IDC_IMG_ROUTE_EDIT, g_imgRoute, sizeof(g_imgRoute));  // 마지막 인자에 256대신 sizeof씀
            EndDialog(hDlg, 0);
            return TRUE;

        case IDCANCEL:
            EndDialog(hDlg, 0);
            return TRUE;

        case IDC_OPEN_FILE_BTN:
        {
            char szFileName[MAX_PATH];
            szFileName[0] = 0;

            if (OpenImage(hDlg, szFileName, sizeof(szFileName), "이미지 파일을 선택하세요", "All Files(*.bmp)\0*.bmp\0") == FALSE) break;
            SetWindowText(g_hEditFileToBeOpened, szFileName);
            return TRUE;
        }
        }
    }

    }
    return FALSE;
}



//-----------------------------------------------------------------------------
//      텍스트 다이얼로그를 호출합니다
//-----------------------------------------------------------------------------
BOOL CALLBACK TextDialogBoxProc(HWND hDlg, UINT iMessage, WPARAM wParam, LPARAM lParam)
{
    switch (iMessage)
    {
    case WM_INITDIALOG:
        CreateComboBox(hDlg);
        return TRUE;

    case WM_PAINT:
        PAINTSTRUCT ps;
        BeginPaint(hDlg, &ps);
        EndPaint(hDlg, &ps);
        return TRUE;

    case WM_COMMAND:
    {
        switch (wParam)
        {
        case IDCOLOR:
            ChoiceTextColor(hDlg);
            return 0;

        case IDOK:
            SaveTextInfo(hDlg);
            EndDialog(hDlg, 0);
            return TRUE;

        case IDCANCEL:
            EndDialog(hDlg, 0);
            return TRUE;
        }
    }

    }
    return FALSE;
}



//-----------------------------------------------------------------------------
//      '이미지 선택' 메뉴를 클릭했을 때 실행하는 함수
//-----------------------------------------------------------------------------
void WINAPI OpenImgProc(HWND hWnd)
{
    DialogBox(hInst, MAKEINTRESOURCE(IDD_ADD_IMG_DLG), hWnd, ImageDialogBoxProc);

    if (g_hImageLoaded) DeleteObject(g_hImageLoaded);
    LoadBmpImage();
    InvalidateRect(hWnd, NULL, TRUE); // 무효화를 해야 WM_PAINT 메시지가 다시 발생한다.
}



//-----------------------------------------------------------------------------
//      '텍스트 추가 및 수정' 메뉴를 클릭했을 때 실행하는 함수
//-----------------------------------------------------------------------------
void WINAPI ID_AddTextProc(HWND hWnd)
{
    if (g_textCnt < 3)
    {
        printf("ID_AddTextProc 실행 시작\n");
        DialogBox(hInst, MAKEINTRESOURCE(IDD_ADD_TEXT_DLG), hWnd, TextDialogBoxProc);
        InvalidateRect(hWnd, NULL, TRUE); // WndProc 내 WM_PAINT 메시지가 다시 발생한다.
        printf("ID_AddTextProc 실행 종료\n");
    }
    else
        MessageBox(hWnd, "텍스는 3개까지 추가가 가능합니다.", "알림", MB_OK);
}



//-----------------------------------------------------------------------------
//      메인 윈도우 메세지 처리
//-----------------------------------------------------------------------------
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    MousePanning(hWnd, message, wParam, lParam);
    POINT P;

    switch (message)
    {
    case WM_CREATE:         //윈도우가 생성될 때 한번 옴
        lstrcpy(g_imgRoute, "girl.bmp"); LoadBmpImage();  //테스트용임
        return 0;

    case WM_DESTROY:        //윈도우가 파기될 때
        if (g_hImageLoaded) DeleteObject(g_hImageLoaded);
        PostQuitMessage(0); //GetMessage()의 리턴을 FALSE로 만들어 종료하게 함
        return 0;

    case WM_PAINT:          //화면을 그려야 할 이유가 생겼을 떄
        PAINTSTRUCT PS;
        //DrawTextAll(hWnd);
        BeginPaint(hWnd, &PS);
        DrawAll(hWnd, PS.hdc);
        EndPaint(hWnd, &PS);
        return 0;

    case WM_SIZE:           //윈도우 크기의 변화가 생겼을 때
                            //LOWORD(lPrm): Client Width, HIWORD(lPrm): Client Height
        return 0;

    case WM_COMMAND:        //메뉴를 클릭했을 때
        switch (LOWORD(wParam))
        {
        case ID_ADD_IMG:
            OpenImgProc(hWnd); break;

        case ID_ADD_TEXT:
            ID_AddTextProc(hWnd); break;

        case IDM_EXIT:
            DestroyWindow(hWnd); break;
        }
        return 0;

    case WM_SETCURSOR:
        GetCursorPos(&P);
        ScreenToClient(hWnd, &P); // 노트북 화면이 기준이 아니라 윈도우 메인 화면을 기준으로 한다.
        switch (GetDragingMode(hWnd, P))
        {
        case VERTEX_ADJUSTWIDTH:      SetCursor(LoadCursor(NULL, IDC_SIZEWE));		return TRUE;
        case VERTEX_ADJUSTHEIGHT:     SetCursor(LoadCursor(NULL, IDC_SIZENS));		return TRUE;
        case VERTEX_ADJUSTDIAGONAL:	  SetCursor(LoadCursor(NULL, IDC_SIZENWSE));	return TRUE;
        case VERTEX_INTEXT_1:		  SetCursor(LoadCursor(NULL, IDC_HAND));		return TRUE;
        case VERTEX_INTEXT_2:		  SetCursor(LoadCursor(NULL, IDC_HAND));		return TRUE;
        case VERTEX_INTEXT_3:		  SetCursor(LoadCursor(NULL, IDC_HAND));		return TRUE;
        }
        break;

    case WM_KEYDOWN:   
        KeyProc(hWnd, wParam);
        return 0;
    }

    return DefWindowProc(hWnd, message, wParam, lParam);
}



//-----------------------------------------------------------------------------
//      윈도우 프로그래밍 기본 함수
//-----------------------------------------------------------------------------
ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style          = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc    = WndProc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = hInstance;
    wcex.hIcon          = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_BANNERT3));
    wcex.hCursor        = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground  = (HBRUSH)(COLOR_WINDOW+1);
    wcex.lpszMenuName   = MAKEINTRESOURCEW(IDC_BANNERT3);
    wcex.lpszClassName  = szWindowClass;
    wcex.hIconSm        = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassExW(&wcex);
}



//-----------------------------------------------------------------------------
//      윈도우 프로그래밍 기본 함수
//-----------------------------------------------------------------------------
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
   hInst = hInstance; // 인스턴스 핸들을 전역 변수에 저장합니다.

   HWND hWnd = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
      CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, hInstance, nullptr);

   if (!hWnd)
   {
      return FALSE;
   }

   ShowWindow(hWnd, nCmdShow);
   UpdateWindow(hWnd);

   return TRUE;
}



//-----------------------------------------------------------------------------
//      WIN32 API 어플리케이션 메인
//-----------------------------------------------------------------------------
int APIENTRY wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine, _In_ int nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    // Windows 프로그래밍에서 콘솔을 출력하는 소스코드
    AllocConsole();
    freopen("CONOUT$", "wt", stdout);
    //CONOUT$ - console 창
    //wt - 텍스트 쓰기 모드
    //stdout - 출력될 파일 포인터(모니터로 지정)

    // 전역 문자열을 초기화합니다.
    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_BANNERT3, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);

    // 애플리케이션 초기화를 수행합니다:
    if (!InitInstance(hInstance, nCmdShow))
    {
        return FALSE;
    }

    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_BANNERT3));

    MSG msg;

    // 기본 메시지 루프입니다:
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    return (int)msg.wParam;
}