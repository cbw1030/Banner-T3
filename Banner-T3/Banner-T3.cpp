﻿#define _CRT_SECURE_NO_WARNINGS
#include "framework.h"
#include <CommDlg.h>
#include <math.h>
#include "resource.h"
#include <stdio.h>
#include <string.h>
#include "Banner-T3.h"
#include "d2d1.h"

//-------------------------------------------------------------------------------------------------
// Macro Definitions
#define MAX_LOADSTRING 100
#define RADIUS 5                               // 이미지 꼭짓점 반지름

#define countof(strucName) (sizeof(strucName) / sizeof(strucName[0]))

#define VERTEX_NOSELECTED       -2
#define VERTEX_INTEXT_1         -3
#define VERTEX_INTEXT_2         -4
#define VERTEX_INTEXT_3         -5

#define VERTEX_ADJUSTWIDTH      0
#define VERTEX_ADJUSTHEIGHT     1
#define VERTEX_ADJUSTDIAGONAL   2
#define VERTEX_IMAGE_LENGTH     3

#define PAPERXSIZE  2100                        // A4 가로 mm
#define PAPERYSIZE  2970                        // A4 세로 mm
#define CL 5                                    // 이미지 테두리
#define ZOOMBASE    1000
#define MAX_TEXT    10                          // 추가할 수 있는 텍스트 최대 개수
#define MAX_IMAGE   10                          // 추가할 수 있는 이미지 최대 개수

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
static int g_imgSzX, g_imgSzY;                  // Work영역 상의 이미지 크기

static HWND g_hButtonOpenFileDialog;            // 파일열기 대화상자를 실행하기 위한 버튼의 핸들
static HWND g_hEditFileToBeOpened;              // 파일의 경로와 이름을 가져오는 에디트 컨트롤의 핸들
static char g_imgRoute[256];                    // 이미지 경로 ex)"C:\project\Banner\banner\girl.bmp"

static int      g_textSz[3];                    // 사용자가 콤보박스에서 선택한 글자 크기(pt)
static char*    g_textFontArr[3];               // 사용자가 선택한 글꼴을 모아놓은 배열
static char*    g_textStrArr[3];                // 사용자가 입력한 텍스트를 모아놓은 배열
static int      g_textCnt = 0;                  // 텍스트 개수
static RGBQUAD  g_textColor[3];                 // 사용자가 선택한 글자 색상

//작업중인 이미지 관련
static HPEN     g_hPenPapaer;
static HBITMAP  g_hImageLoaded;                 // 로딩된 HBITMAP
static RECT     g_imgRect;                      // 이미지 좌표

//static RECT     g_imgRectArr[MAX_IMAGE];
//static BITMAP   g_bmArr[MAX_IMAGE];
//static HBITMAP  g_hImageLoadedArr[MAX_IMAGE];
//static int      g_imgCnt = 0;

//작업중인 텍스트 관련
static RECT g_textRect[3];                      // 텍스트 외곽 좌표
static int  g_prevTextSz;                       // 미리보기에서 텍스트 크기

//작업중인 A4 관련
static int PAPERXQTY;                           // 사용자가 입력할 A4용지 가로 장 수
static int PAPERYQTY;                           // 사용자가 입력할 A4용지 세로 장 수

//-----------------------------------------------------------------------------
//      PrinterDC를 호출합니다
//-----------------------------------------------------------------------------
HDC WINAPI GetPrinterDC(HWND Hwnd)
{
    BOOL Rslt;
    PRINTDLG pd;

    memset(&pd, 0, sizeof(PRINTDLG));
    pd.lStructSize = sizeof(pd);
    pd.hwndOwner = Hwnd;
    pd.Flags = PD_RETURNDC; // PD_ALLPAGES | PD_RETURNDC | PD_NOSELECTION | PD_ENABLEPRINTTEMPLATE | PD_ENABLEPRINTHOOK

    // Retrieves the printer DC
    Rslt = PrintDlg(&pd);
    return Rslt ? pd.hDC : NULL;
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
int W2D(int R) { return MulDiv(R, g_zoomVal, ZOOMBASE); }                      // 크기 변환하는 함수
int W2DX(int X) { return MulDiv(X - g_panningX, g_zoomVal, ZOOMBASE); }         // 이미지 부분에 사용(StretchBlt)
int W2DY(int Y) { return MulDiv(Y - g_panningY, g_zoomVal, ZOOMBASE); }         // 이미지 부분에 사용(StretchBlt)

int D2W(int R) { return MulDiv(R, ZOOMBASE, g_zoomVal); }
int D2WX(int X) { return MulDiv(X, ZOOMBASE, g_zoomVal) + g_panningX; }
int D2WY(int Y) { return MulDiv(Y, ZOOMBASE, g_zoomVal) + g_panningY; }



//-----------------------------------------------------------------------------
//      메인 윈도우의 이미지 테두리를 클릭했을 때, 경계선을 그립니다.
//-----------------------------------------------------------------------------
void WINAPI DrawImgBoundaryLine(HDC hdc)
{
    SelectObject(hdc, GetStockObject(NULL_BRUSH));  //면을 칠하지 않도록 함
    Rectangle(hdc, W2DX(0), W2DY(0), W2DX(g_imgSzX), W2DY(g_imgSzY));
}



//-----------------------------------------------------------------------------
//      Bmp파일을 HBITMAP로 로딩하여 리턴
//-----------------------------------------------------------------------------
void WINAPI LoadBmpImage(void)
{
    BITMAP bm;

    if ((g_hImageLoaded =  (HBITMAP)LoadImage(NULL, g_imgRoute, IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION)) != NULL)
    {
        GetObject(g_hImageLoaded, sizeof(BITMAP), &bm);

        // GetSystemMetrics();
        g_imgSzX = (int)((bm.bmWidth / 25.4) * 280);      
        g_imgSzY = (int)((bm.bmHeight / 25.4) * 280);
    }

    // 이미지 여러 개 추가하는 소스코드
    /*
    BITMAP bm;

    if ((g_hImageLoadedArr[g_imgCnt] =
        (HBITMAP)LoadImage(NULL, g_imgRoute, IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION)) != NULL)
    {
        GetObject(g_hImageLoadedArr[g_imgCnt], sizeof(BITMAP), &g_bmArr[g_imgCnt]);

        g_imgSzX = (int)((g_bmArr[g_imgCnt].bmWidth / 25.4) * 120);       // 120은 이미지의 dpi를 의미한다(일단 정적으로 해놓음)
        g_imgSzY = (int)((g_bmArr[g_imgCnt].bmHeight / 25.4) * 120);
    }
    */
}



//-----------------------------------------------------------------------------
//       원본이미지를 크기조절 후 화면에 출력
//-----------------------------------------------------------------------------
void WINAPI DrawStretchBitmap(HDC hDC, HBITMAP hBtm, int X1, int Y1, int X2, int Y2)
{
    HDC     hMemDC;
    HBITMAP hBtmOld;
    BITMAP  bm;

    GetObject(hBtm, sizeof(BITMAP), &bm);

    hMemDC = CreateCompatibleDC(hDC);
    hBtmOld = (HBITMAP)SelectObject(hMemDC, hBtm);                                          // hBtm가 선택되기 전의 핸들을 저장해 둔다
    StretchBlt(hDC, X1, Y1, X2 - X1, Y2 - Y1, hMemDC, 0, 0, bm.bmWidth, bm.bmHeight, SRCCOPY);
    SelectObject(hMemDC, hBtmOld);                                                          // hImage 선택을 해제하기 위해 hBtmOld을 선택한다
    DeleteDC(hMemDC);

    // 이미지 여러 개 추가하는 소스코드
    /*
    HDC     hMemDCArr[MAX_IMAGE];
    HBITMAP hBtmOldArr[MAX_IMAGE];

    GetObject(hBtm, sizeof(BITMAP), &g_bmArr[g_imgCnt]);

    hMemDCArr[g_imgCnt] = CreateCompatibleDC(hDC);
    hBtmOldArr[g_imgCnt] = (HBITMAP)SelectObject(hMemDCArr[g_imgCnt], hBtm);                                          // hBtm가 선택되기 전의 핸들을 저장해 둔다
    StretchBlt(hDC, X1, Y1, X2 - X1, Y2 - Y1, hMemDCArr[g_imgCnt], 0, 0, g_bmArr[g_imgCnt].bmWidth, g_bmArr[g_imgCnt].bmHeight, SRCCOPY);
    SelectObject(hMemDCArr[g_imgCnt], hBtmOldArr[g_imgCnt]);                                                          // hImage 선택을 해제하기 위해 hBtmOld을 선택한다
    DeleteDC(hMemDCArr[g_imgCnt]);
    */
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
//      글꼴, 색상는 여기서 모두 작업합니다
//-----------------------------------------------------------------------------
void WINAPI SaveTextInfo(HWND hWnd)
{
    HDC   hDC;
    HFONT hFont, oldFont;
    SIZE  S;

    if (g_textStrArr[g_textCnt] == NULL)
    {
        g_textSz[g_textCnt] = GetDlgItemInt(hWnd, IDSIZE, NULL, FALSE) * 10;
        g_textStrArr[g_textCnt] = (char*)malloc(1024);
        g_textFontArr[g_textCnt] = (char*)malloc(100);
        GetDlgItemText(hWnd, IDSTRING, g_textStrArr[g_textCnt], 1024);
        GetDlgItemText(hWnd, IDFONT, g_textFontArr[g_textCnt], 100);

        hDC = GetDC(hWnd);
        hFont = MyCreateFont(g_textSz[g_textCnt], FALSE, FALSE, g_textFontArr[g_textCnt]);
        oldFont = (HFONT)SelectObject(hDC, hFont);

        GetTextExtentPoint32(hDC, g_textStrArr[g_textCnt], lstrlen(g_textStrArr[g_textCnt]), &S);
        SetRect(&g_textRect[g_textCnt], 0, 0, S.cx, S.cy);

        DeleteObject(SelectObject(hDC, oldFont));
        ReleaseDC(hWnd, hDC);
    }
}



//-----------------------------------------------------------------------------
//      16진수 -> RGB로 변환합니다
//-----------------------------------------------------------------------------
RGBQUAD colorConverter(int hexValue)
{
    RGBQUAD rgbColor;
    rgbColor.rgbRed = ((hexValue >> 16) & 0xFF);    // Extract the RR byte
    rgbColor.rgbGreen = ((hexValue >> 8) & 0xFF);   // Extract the GG byte
    rgbColor.rgbBlue = ((hexValue) & 0xFF);         // Extract the BB byte

    return rgbColor;
}



//-----------------------------------------------------------------------------
//      글자 색상을 처리합니다.
//-----------------------------------------------------------------------------
void WINAPI ChoiceTextColor(HWND hWnd)
{
    COLORREF textColor; // 사용자가 선택한 색상
    COLORREF crTemp[16];
    CHOOSECOLOR col;

    memset(&col, 0, sizeof(CHOOSECOLOR));
    col.lStructSize = sizeof(CHOOSECOLOR);
    col.hwndOwner = hWnd;

    col.lpCustColors = crTemp;
    col.Flags = 0;

    if (ChooseColor(&col) != 0)
    {
        textColor = col.rgbResult;
        g_textColor[g_textCnt] = colorConverter(textColor);
    }
}



//-----------------------------------------------------------------------------
//      글꼴, 글자크기 콤보박스는 여기서 작업합니다
//-----------------------------------------------------------------------------
void WINAPI CreateComboBox(HWND hWnd)
{
    HWND fontDlg, szDlg;
    BOOL fontInit, szInit;

    char fonts[][100] = { "궁서", "굴림", "돋움", "나눔고딕", "HY견고딕" };
    char sz[][100] = { "10", "11", "12", "13", "14", "16", "18", "20", "24", "28", "32", "36", "40", "48", "72", "96", "120" };

    fontInit = SetDlgItemText(hWnd, IDFONT, fonts[0]);
    szInit = SetDlgItemText(hWnd, IDSIZE, sz[0]);

    fontDlg = GetDlgItem(hWnd, IDFONT);
    szDlg = GetDlgItem(hWnd, IDSIZE);

    for (int i = 0; i < 5; i++)
        SendMessage(fontDlg, CB_ADDSTRING, 0, (LPARAM)fonts[i]);

    for (int i = 0; i < 17; i++)
        SendMessage(szDlg, CB_ADDSTRING, 0, (LPARAM)sz[i]);

    SetFocus(GetDlgItem(hWnd, IDSTRING)); // 적용이 왜 안되지..
}



//-----------------------------------------------------------------------------
//      윈도우 영역에서 글꼴, 색상, 크기는 여기서 모두 작업합니다
//-----------------------------------------------------------------------------
void WINAPI DrawTextAll(HWND hWnd, HDC hDC)
{
    int   i;
    RECT  R;
    HFONT hFont, oldFont;

    for (i = 0; i < g_textCnt; i++)
    {
        SetRect(&R, W2DX(g_textRect[i].left), W2DY(g_textRect[i].top), W2DX(g_textRect[i].right), W2DY(g_textRect[i].bottom));

        hFont = MyCreateFont(W2D(g_textSz[i]), FALSE, FALSE, g_textFontArr[i]); // {2: Bold, 3: Italic}
        oldFont = (HFONT)SelectObject(hDC, hFont);

        SetTextColor(hDC, RGB(g_textColor[i].rgbBlue, g_textColor[i].rgbGreen, g_textColor[i].rgbRed));
        SetBkMode(hDC, TRANSPARENT);

        DrawTextEx(hDC, g_textStrArr[i], -1, &R, DT_VCENTER | DT_WORDBREAK, NULL);      //실제 문자열을 찍는 문장. rt에 출력한다.

        SelectObject(hDC, oldFont);
        DeleteObject(hFont);
    }
}




//-----------------------------------------------------------------------------
//      A4 미리보기 선을 그리는 함수
//-----------------------------------------------------------------------------
void WINAPI DrawPaper(HWND hWnd, HDC hDC)
{
    int X, Y, px, py;

    SelectObject(hDC, g_hPenPapaer);
    SelectObject(hDC, GetStockObject(NULL_BRUSH));

    for (Y = 0; Y < PAPERYQTY; Y++)
    {
        for (X = 0; X < PAPERXQTY; X++)
        {
            px = X * PAPERXSIZE;
            py = Y * PAPERYSIZE;
            Rectangle(hDC, W2DX(px), W2DY(py), W2DX(px + PAPERXSIZE), W2DY(py + PAPERYSIZE));
        }
    }
}



//-----------------------------------------------------------------------------
//      모든 화면 그리는 동작
//-----------------------------------------------------------------------------
void WINAPI DrawAll(HWND hWnd, HDC hDC)
{
    DrawStretchBitmap(hDC, g_hImageLoaded, W2DX(0), W2DY(0), W2DX(g_imgSzX), W2DY(g_imgSzY));
    DrawTextAll(hWnd, hDC);

    // 이미지 여러 개
    //DrawStretchBitmap(hDC, g_hImageLoadedArr[g_imgCnt], W2DX(0), W2DY(0), W2DX(g_imgSzX), W2DY(g_imgSzY));
}



//-----------------------------------------------------------------------------
//      메인 윈도우의 이미지 테두리를 클릭했을 때, 꼭짓점(원)을 그립니다.
//-----------------------------------------------------------------------------
void WINAPI DrawImgVertex(HDC hdc)
{
    DrawCircle(hdc, W2DX(g_imgSzX), (W2DY(g_imgSzY) / 2), RADIUS);      // 우측 모서리 중앙
    DrawCircle(hdc, (W2DX(g_imgSzX) / 2), W2DY(g_imgSzY), RADIUS);      // 아래 모서리 중앙
    DrawCircle(hdc, W2DX(g_imgSzX), W2DY(g_imgSzY), RADIUS);      // 우측 하단 꼭짓점
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
    DrawImgVertex(hdc);        // 이미지 꼭짓점
    InvalidateRect(hWnd, NULL, FALSE); // 적용하면 텍스트 잔상처리가 해결되지만 반짝거리고 안하면 텍스트 잔상처리가 해결안됨
    ReleaseDC(hWnd, hdc);
}



//-----------------------------------------------------------------------------
//      이미지 크기를 조정할 위치정보를 리턴함
//-----------------------------------------------------------------------------
int WINAPI GetDragingMode(HWND hWnd, POINT P)
{
    int  i, DragingMode = VERTEX_NOSELECTED;
    HDC  hdc;
    RECT R;
    static const int TextSelMode[] = { VERTEX_INTEXT_1, VERTEX_INTEXT_2, VERTEX_INTEXT_3 };

    hdc = GetDC(hWnd);

    for (i = 0; i < g_textCnt; i++)
    {
        SetRect(&R, W2DX(g_textRect[i].left), W2DY(g_textRect[i].top), W2DX(g_textRect[i].right), W2DY(g_textRect[i].bottom));
        if (PtInRect(&R, P)) { DragingMode = TextSelMode[i]; goto ProcExit; }
    }

    SetRect(&R, W2DX(g_imgSzX) - CL, W2DY(g_imgSzY) - CL, W2DX(g_imgSzX) + CL, W2DY(g_imgSzY) + CL);
    if (PtInRect(&R, P)) { DragingMode = VERTEX_ADJUSTDIAGONAL; goto ProcExit; }

    SetRect(&R, W2DX(g_imgSzX) - CL, W2DY(0), W2DX(g_imgSzX) + CL, W2DY(g_imgSzY) - CL);
    if (PtInRect(&R, P)) { DragingMode = VERTEX_ADJUSTWIDTH; goto ProcExit; }

    SetRect(&R, W2DX(0), W2DY(g_imgSzY) - CL, W2DX(g_imgSzX) - CL, W2DY(g_imgSzY) + CL);
    if (PtInRect(&R, P)) { DragingMode = VERTEX_ADJUSTHEIGHT; goto ProcExit; }

ProcExit:
    ReleaseDC(hWnd, hdc);
    //printf("DragingMode=%d\n",DragingMode);
    return DragingMode;
}



//-----------------------------------------------------------------------------
//      드래깅 모드를 처리합니다.
//-----------------------------------------------------------------------------
void WINAPI HandleDragingMode(HWND hWnd, int DragingMode, int DX, int DY)
{
    int i;

    DrawSizeInfoLine(hWnd);         //기존 그려진 안내선을 지움(잔상처리)

    switch (DragingMode)
    {
    case VERTEX_ADJUSTWIDTH:    g_imgSzX += DX; break;
    case VERTEX_ADJUSTHEIGHT:   g_imgSzY += DY; break;
    case VERTEX_ADJUSTDIAGONAL: g_imgSzX += DX; g_imgSzY += DY; break;
    case VERTEX_INTEXT_1: i = 0; goto AdjText;
    case VERTEX_INTEXT_2: i = 1; goto AdjText;
    case VERTEX_INTEXT_3: i = 2; //goto AdjText;
    AdjText:
        OffsetRect(&g_textRect[i], DX, DY);
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
    static int currZoom = 4;
    static const int zoomTable[] = { 2000, 1000, 500, 250, 125, 63, 31, 15 };

    switch (Msg)
    {
    case WM_LBUTTONDOWN:
        oldP.x = LoInt16(lPrm);
        oldP.y = HiInt16(lPrm);

        if ((DragingMode = GetDragingMode(hWnd, oldP)) == VERTEX_NOSELECTED) break;
        DrawSizeInfoLine(hWnd); 

        SetCapture(hWnd);
        break;

    case WM_MOUSEMOVE:
        if (GetCapture() == hWnd)
        {
            HandleDragingMode(hWnd, DragingMode, D2W(LoInt16(lPrm) - oldP.x), D2W(HiInt16(lPrm) - oldP.y));
            InvalidateRect(hWnd, NULL, TRUE);
            oldP.x = LoInt16(lPrm);
            oldP.y = HiInt16(lPrm);
        }
        break;

    case WM_LBUTTONUP:
        ReleaseCapture();
        break;

    case WM_MOUSEWHEEL:
        if ((SHORT)HIWORD(wPrm) > 0)                                             //마우스휠을 올릴 경우 '확대'
        {
            if (currZoom > 0)
            {
                g_zoomVal = zoomTable[--currZoom];
                InvalidateRect(hWnd, NULL, TRUE);
            }
        }
        else                                                                    //마우스휠을 내릴 경우 '축소'
        {
            if (currZoom < countof(zoomTable) - 1)
            {
                g_zoomVal = zoomTable[++currZoom];
                InvalidateRect(hWnd, NULL, TRUE);
            }
        }
        break;
    }
}



//----------------------------------------------------------------------------
//      파일을 열어 이미지를 선택합니다.
//----------------------------------------------------------------------------
BOOL WINAPI OpenImage(HWND hWnd, LPSTR Buff, int BuffSize, LPCSTR Title, LPCSTR Filter)
{
    OPENFILENAME ofn;
    BOOL rv; //return value

    ZeroMemory(&ofn, sizeof(OPENFILENAME));
    ofn.lStructSize = sizeof(OPENFILENAME);
    ofn.hwndOwner = hWnd;
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
//      사용자로부터 입력받은 A4 용지 가로/세로 장 수를 처리합니다.
//-----------------------------------------------------------------------------
void WINAPI SaveA4Info(HWND hWnd)
{
    if (GetDlgItemInt(hWnd, IDC_A4_WIDTH_EDIT, NULL, FALSE) == 0 || GetDlgItemInt(hWnd, IDC_A4_HEIGHT_EDIT, NULL, FALSE) == 0)
    {
        MessageBox(hWnd, "공백을 입력해 주세요.", "알림", MB_OK);
    }
    else
    {
        PAPERXQTY = GetDlgItemInt(hWnd, IDC_A4_WIDTH_EDIT, NULL, FALSE);
        PAPERYQTY = GetDlgItemInt(hWnd, IDC_A4_HEIGHT_EDIT, NULL, FALSE);
    }
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
//      이미지, 텍스트, 클립아트를 프린트합니다.
//-----------------------------------------------------------------------------
void WINAPI Print(HWND hWnd)
{
    int X, Y, prtResX, prtResY, orgZoom, orgPanX, orgPanY;
    HDC hPrnDC = NULL;
    DOCINFO di = { sizeof(DOCINFO), TEXT("Printing") };

    orgZoom = g_zoomVal;
    orgPanX = g_panningX;
    orgPanY = g_panningY;

    if ((hPrnDC = GetPrinterDC(hWnd)) == NULL) goto ProcExit;
    StartDoc(hPrnDC, &di);

    prtResX = GetDeviceCaps(hPrnDC, HORZRES);
    prtResY = GetDeviceCaps(hPrnDC, VERTRES);

    g_zoomVal = prtResX * 1000 / PAPERXSIZE;

    for (Y = 0; Y < PAPERYQTY; Y++)
    {
        for (X = 0; X < PAPERXQTY; X++)
        {
            StartPage(hPrnDC);
            g_panningX = X * PAPERXSIZE;
            g_panningY = Y * PAPERYSIZE;
            DrawAll(hWnd, hPrnDC);
            EndPage(hPrnDC);
        }
    }
    EndDoc(hPrnDC);

ProcExit:
    if (hPrnDC) DeleteDC(hPrnDC);
    g_zoomVal = orgZoom;
    g_panningX = orgPanX;
    g_panningY = orgPanY;
}
//-----------------------------------------------------------------------------
//      A4 용지 셋팅 다이얼로그를 호출합니다
//-----------------------------------------------------------------------------
BOOL CALLBACK A4DialogBoxProc(HWND hWnd, UINT iMessage, WPARAM wParam, LPARAM lParam)
{
    switch (iMessage)
    {

    case WM_PAINT:
        PAINTSTRUCT ps;
        BeginPaint(hWnd, &ps);
        EndPaint(hWnd, &ps);
        return TRUE;

    case WM_COMMAND:
    {
        switch (wParam)
        {
        case IDOK:
            SaveA4Info(hWnd);
            EndDialog(hWnd, 0);
            return TRUE;

        case IDCANCEL:
            EndDialog(hWnd, 0);
            return TRUE;
        }
    }

    }
    return FALSE;
}



//-----------------------------------------------------------------------------
//      이미지 다이얼로그를 호출합니다
//-----------------------------------------------------------------------------
BOOL CALLBACK ImageDialogBoxProc(HWND hWnd, UINT iMessage, WPARAM wParam, LPARAM lParam)
{
    switch (iMessage)
    {
    case WM_INITDIALOG:
    {
        g_hButtonOpenFileDialog = GetDlgItem(hWnd, IDC_OPEN_FILE_BTN);
        g_hEditFileToBeOpened = GetDlgItem(hWnd, IDC_IMG_ROUTE_EDIT);
        return TRUE;
    }

    case WM_PAINT:
        PAINTSTRUCT ps;
        BeginPaint(hWnd, &ps);
        EndPaint(hWnd, &ps);
        return TRUE;

    case WM_COMMAND:
    {
        switch (wParam)
        {
        case IDOK:
            GetDlgItemText(hWnd, IDC_IMG_ROUTE_EDIT, g_imgRoute, sizeof(g_imgRoute));
            EndDialog(hWnd, 0);
            return TRUE;

        case IDCANCEL:
            EndDialog(hWnd, 0);
            return TRUE;

        case IDC_OPEN_FILE_BTN:
        {
            char szFileName[MAX_PATH];
            szFileName[0] = 0;

            if (OpenImage(hWnd, szFileName, sizeof(szFileName), "이미지 파일을 선택하세요", "All Files(*.bmp)\0*.bmp\0") == FALSE) break;
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
BOOL CALLBACK TextDialogBoxProc(HWND hWnd, UINT iMessage, WPARAM wParam, LPARAM lParam)
{
    switch (iMessage)
    {
    case WM_INITDIALOG:
        CreateComboBox(hWnd);
        return TRUE;

    case WM_PAINT:
        PAINTSTRUCT ps;
        BeginPaint(hWnd, &ps);
        EndPaint(hWnd, &ps);
        return TRUE;

    case WM_COMMAND:
    {
        switch (wParam)
        {
        case IDCOLOR:
            ChoiceTextColor(hWnd);
            return 0;

        case IDOK:
            SaveTextInfo(hWnd);
            EndDialog(hWnd, IDOK);
            return TRUE;

        case IDCANCEL:
            EndDialog(hWnd, IDCANCEL);
            return TRUE;
        }
    }

    }
    return FALSE;
}



//-----------------------------------------------------------------------------
//      '이미지 추가' 메뉴를 클릭했을 때 실행하는 함수
//-----------------------------------------------------------------------------
void WINAPI AddImgProc(HWND hWnd)
{
    DialogBox(hInst, MAKEINTRESOURCE(IDD_ADD_IMG_DLG), hWnd, ImageDialogBoxProc);

    if (g_hImageLoaded) DeleteObject(g_hImageLoaded);
    //if (g_hImageLoadedArr[g_imgCnt]) DeleteObject(g_hImageLoadedArr[g_imgCnt]);
    LoadBmpImage();
    InvalidateRect(hWnd, NULL, TRUE); // 무효화를 해야 WM_PAINT 메시지가 다시 발생한다.
}



//-----------------------------------------------------------------------------
//      '텍스트 추가' 메뉴를 클릭했을 때 실행하는 함수
//-----------------------------------------------------------------------------
void WINAPI AddTextProc(HWND hWnd)
{
    if (g_textCnt < 3)
    {
        printf("AddTextProc 실행 시작\n");
        if (DialogBox(hInst, MAKEINTRESOURCE(IDD_ADD_TEXT_DLG), hWnd, TextDialogBoxProc) == IDOK) g_textCnt++;
        InvalidateRect(hWnd, NULL, TRUE); // WndProc 내 WM_PAINT 메시지가 다시 발생한다.
        printf("AddTextProc 실행 종료\n");
    }
    else
        MessageBox(hWnd, "텍스는 3개까지 추가가 가능합니다.", "알림", MB_OK);
}



//-----------------------------------------------------------------------------
//      '클립아트 추가' 메뉴를 클릭했을 때 실행하는 함수
//-----------------------------------------------------------------------------
void WINAPI AddClipArtProc(HWND hWnd)
{
    
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
        DialogBox(hInst, MAKEINTRESOURCE(IDD_INIT_DLG), hWnd, A4DialogBoxProc); // 사용자로부터 A4 장 수 입력 받음
        lstrcpy(g_imgRoute, "girl.bmp"); LoadBmpImage();                        //테스트용임
        g_hPenPapaer = CreatePen(PS_SOLID, 1, RGB(109, 202, 185));
        return 0;

    case WM_DESTROY:        //윈도우가 파기될 때
        if (g_hImageLoaded) DeleteObject(g_hImageLoaded);
        if (g_hPenPapaer)   DeleteObject(g_hPenPapaer);
        PostQuitMessage(0); //GetMessage()의 리턴을 FALSE로 만들어 종료하게 함
        return 0;

    case WM_PAINT:          //화면을 그려야 할 이유가 생겼을 떄
        PAINTSTRUCT PS;
        BeginPaint(hWnd, &PS);
        DrawAll(hWnd, PS.hdc);
        DrawPaper(hWnd, PS.hdc);
        EndPaint(hWnd, &PS);
        return 0;

    case WM_COMMAND:        //메뉴를 클릭했을 때
        switch (LOWORD(wParam))
        {
        case ID_ADD_IMG:
            AddImgProc(hWnd); break;

        case ID_ADD_TEXT:
            AddTextProc(hWnd); break;

        case ID_ADD_CLIPART:
            AddClipArtProc(hWnd); break;

        case ID_PRINT:
            Print(hWnd); break;

        case IDM_EXIT:
            DestroyWindow(hWnd); break;
        }
        return 0;
  
    case WM_SETCURSOR:
        GetCursorPos(&P);
        ScreenToClient(hWnd, &P); // 노트북 화면이 기준이 아니라 윈도우 메인 화면을 기준으로 함
        switch (GetDragingMode(hWnd, P))
        {
        case VERTEX_ADJUSTWIDTH:      SetCursor(LoadCursor(NULL, IDC_SIZEWE));      return TRUE;
        case VERTEX_ADJUSTHEIGHT:     SetCursor(LoadCursor(NULL, IDC_SIZENS));      return TRUE;
        case VERTEX_ADJUSTDIAGONAL:   SetCursor(LoadCursor(NULL, IDC_SIZENWSE));    return TRUE;
        case VERTEX_INTEXT_1:         SetCursor(LoadCursor(NULL, IDC_HAND));        return TRUE;
        case VERTEX_INTEXT_2:         SetCursor(LoadCursor(NULL, IDC_HAND));        return TRUE;
        case VERTEX_INTEXT_3:         SetCursor(LoadCursor(NULL, IDC_HAND));        return TRUE;
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

    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_BANNERT3));
    wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
    //wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.hbrBackground = (HBRUSH)CreateSolidBrush(RGB(236, 244, 243));
    wcex.lpszMenuName = MAKEINTRESOURCEW(IDC_BANNERT3);
    wcex.lpszClassName = szWindowClass;
    wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassExW(&wcex);
}



//-----------------------------------------------------------------------------
//      윈도우 프로그래밍 기본 함수
//-----------------------------------------------------------------------------
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
    hInst = hInstance; // 인스턴스 핸들을 전역 변수에 저장합니다.

    HWND hWnd = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, NULL, NULL, hInstance, NULL);

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
int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
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
    while (GetMessage(&msg, NULL, 0, 0))
    {
        if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    return (int)msg.wParam;
}