#define _CRT_SECURE_NO_WARNINGS
#include "framework.h"
#include <CommDlg.h>
#include <math.h>
#include "resource.h"
#include <stdio.h>
#include "Banner-T3.h"

//-------------------------------------------------------------------------------------------------
// Macro Definitions
#define MAX_LOADSTRING 100                      
#define RADIUS 5                                // 이미지 꼭짓점 반지름

#define COUNTOF(strucName) (sizeof(strucName) / sizeof(strucName[0]))   // 배열 해당 원소의 크기를 구하기 위함

#define ADJUST_WIDTH      1                     // 이미지 오른쪽 테두리
#define ADJUST_HEIGHT     2                     // 이미지 하단 테두리
#define ADJUST_DIAGONAL   3                     // 이미지 오른쪽 하단 꼭짓점

#define NO_SELECTED       0                     // 이미지를 선택하지 않은 경우
#define IN_TEXT_1         10                    // 첫 번째 이미지를 선택한 경우
#define IN_TEXT_2         20                    // 두 번째 이미지를 선택한 경우
#define IN_TEXT_3         30                    // 세 번째 이미지를 선택한 경우

#define IN_CLIPART        100                   // 첫 번째 클립아트 내부(위치 조정)를 선택한 경우
#define LINE_CLIPART      200                   // 첫 번째 클립아트 경계선(크기 조절 위함)을 선택한 경우

#define PAPER_X_SIZE  2100                      // A4 가로 mm
#define PAPER_Y_SIZE  2970                      // A4 세로 mm
#define CL 5                                    // 마우스로 이미지 늘이거나 줄이기 위해 hover했을 때 커서 바뀌게 하기 위함
#define ZOOMBASE    1000                        // 이미지 확대 및 축소하는 기준점의 초깃값
#define MAX_TEXT    10                          // 추가할 수 있는 텍스트 최대 개수
#define MAX_IMAGE   10                          // 추가할 수 있는 이미지 최대 개수

typedef const unsigned char * LPCBYTE;
typedef const int           *  LPCINT;
typedef short               * LPINT16;
typedef const short         * LPCINT16;

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

//작업중인 텍스트 관련
static RECT     g_textRect[3];                  // 텍스트 외곽 좌표
static int      g_prevTextSz;                   // 미리보기에서 텍스트 크기
static int      g_textSz[3];                    // 사용자가 콤보박스에서 선택한 글자 크기(pt)
static char*    g_textFontArr[3];               // 사용자가 선택한 글꼴을 모아놓은 배열
static char*    g_textStrArr[3];                // 사용자가 입력한 텍스트를 모아놓은 배열
static int      g_textCnt = 0;                  // 텍스트 개수
static RGBQUAD  g_textColor[3];                 // 사용자가 선택한 글자 색상

//작업중인 이미지 관련
static HPEN     g_hPenPapaer;                   // 작업영역 A4 그리드의 색상을 적용하기 위함
static char     g_imgRoute[256];                // 이미지 경로 ex)"C:\project\Banner\banner\girl.bmp"
static HBITMAP  g_hImageLoaded;                 // 로딩된 HBITMAP
static RECT     g_imgRect;                      // 이미지 좌표
//static RECT     g_imgRectArr[MAX_IMAGE];
//static BITMAP   g_bmArr[MAX_IMAGE];
//static HBITMAP  g_hImageLoadedArr[MAX_IMAGE];
//static int      g_imgCnt = 0;

// 작업중인 클립아트 관련;
static HENHMETAFILE g_hEnh;    
static char         g_clipArtRoute[256];        // 클립아트 경로
static int          g_clipArtSzX = 2100;
static int          g_clipArtSzY = 2100;
static RECT         g_clipArtRect;              // 클립아트를 그리는 좌표

//작업중인 A4 관련
static bool g_A4Status = FALSE;                 // 사용자로부터 A4 장 수를 올바르게 입력받았는지 '상태'를 나타내는 변수
static int  g_PAPERXQTY;                        // 사용자가 입력할 A4용지 가로 장 수
static int  g_PAPERYQTY;                        // 사용자가 입력할 A4용지 세로 장 수

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
//      16비트 메타 파일을 32비트 메타 파일로 변환하여 메타 파일 핸들을 리턴한다. 
//      에러 발생시 NULL을 리턴한다.
//-----------------------------------------------------------------------------
HENHMETAFILE ConvertWinToEnh(LPTSTR wmf)
{
    HMETAFILE       wfile;
    DWORD           dwSize;
    LPBYTE          pBits;
    METAFILEPICT    mp;
    HDC             hdc;

    // 16비트 메타 파일을 읽고 메타 파일 크기만큼 메모리를 할당한다.
    wfile = GetMetaFile(wmf);
    if (wmf == NULL)
        return NULL;
    dwSize = GetMetaFileBitsEx(wfile, 0, NULL);

    if (dwSize == 0) {
        DeleteMetaFile(wfile);
        return NULL;
    }
    pBits = (LPBYTE)malloc(dwSize);

    // 메타 파일의 내용을 버퍼로 읽어들인다.
    GetMetaFileBitsEx(wfile, dwSize, pBits);
    mp.mm   = MM_ANISOTROPIC;
    mp.xExt = 1000;
    mp.yExt = 1000;
    mp.hMF  = NULL;

    // 32비트 메타 파일을 만든다.
    hdc     = GetDC(NULL);
    g_hEnh  = SetWinMetaFileBits(dwSize, pBits, hdc, &mp);
    ReleaseDC(NULL, hdc);
    DeleteMetaFile(wfile);
    free(pBits);
    return g_hEnh;
}



#pragma pack(push)
#pragma pack(2)
typedef struct
{
    DWORD		dwKey;
    WORD		hmf;
    SMALL_RECT	bbox;           // long이 아닌 short로 선언되어 있음
    WORD		wInch;
    DWORD		dwReserved;
    WORD		wCheckSum;
} APMHEADER, * PAPMHEADER;
#pragma pack(pop)
//-----------------------------------------------------------------------------
//      플레이스블 메타 파일을 32비트 메타 파일로 변경해 준다. 
//      에러 발생시 NULL을 리턴한다.
//-----------------------------------------------------------------------------
HENHMETAFILE ConvertPlaToEnh(LPTSTR szFileName)
{
    DWORD			dwSize;
    LPBYTE			pBits;
    METAFILEPICT	mp;
    HDC				hdc;
    HANDLE			hFile;

    // 32비트 메타 파일이 아니면 플레이스블 메타 파일로 읽는다.
    // 파일 크기만큼 메모리를 할당하고 메타 파일을 읽어들인다.
    hFile = CreateFile(szFileName, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

    if (hFile == INVALID_HANDLE_VALUE)
        return NULL;
    dwSize = GetFileSize(hFile, NULL);
    pBits = (LPBYTE)malloc(dwSize);
    ReadFile(hFile, pBits, dwSize, &dwSize, NULL);
    CloseHandle(hFile);

    // Placeable 메타 파일이 맞는지 확인한다.
    if (((PAPMHEADER)pBits)->dwKey != 0x9ac6cdd7l) {
        free(pBits);
        return NULL;
    }

    // 구조체를 채운다.
    mp.mm = MM_ANISOTROPIC;
    mp.xExt = ((PAPMHEADER)pBits)->bbox.Right - ((PAPMHEADER)pBits)->bbox.Left;
    mp.xExt = (mp.xExt * 2540l) / (DWORD)(((PAPMHEADER)pBits)->wInch);
    mp.yExt = ((PAPMHEADER)pBits)->bbox.Bottom - ((PAPMHEADER)pBits)->bbox.Top;
    mp.yExt = (mp.yExt * 2540l) / (DWORD)(((PAPMHEADER)pBits)->wInch);
    mp.hMF = NULL;

    // 메타 파일을 만든다.
    hdc = GetDC(NULL);
    g_hEnh = SetWinMetaFileBits(dwSize, &(pBits[sizeof(APMHEADER)]), hdc, &mp);
    ReleaseDC(NULL, hdc);
    free(pBits);
    return g_hEnh;
}



//-----------------------------------------------------------------------------
//      32비트 메타 파일의 핸들을 리턴한다. 16비트 메타 파일이나 플레이스블 메타 파일
//      일 경우 32비트 메타 파일로 변환해준다.
//-----------------------------------------------------------------------------
HENHMETAFILE ReadMeta(LPTSTR FileName)
{
    // 32비트 메타 파일의 핸들을 구해 리턴한다.
    g_hEnh = GetEnhMetaFile(FileName);
    if (g_hEnh != NULL) return g_hEnh;

    // 32비트 메타 파일이 아닐 경우 16비트 포멧으로 읽어보고 32비트 전환한다.
    g_hEnh = ConvertWinToEnh(FileName);
    if (g_hEnh != NULL) return g_hEnh;

    // 16비트 메타 파일도 아닐 경우 플레이스블 메타 파일을 32비트로 전환한다.
    g_hEnh = ConvertPlaToEnh(FileName);
    if (g_hEnh != NULL) return g_hEnh;

    // 세 경우 다 해당하지 않을 경우 NULL을 리턴한다.
    return NULL;
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
int W2D(int R) { return MulDiv(R, g_zoomVal, ZOOMBASE); }                       // WorkSpace to Device(크기 변환하는 함수)
int W2DX(int X) { return MulDiv(X - g_panningX, g_zoomVal, ZOOMBASE); }         // 이미지 부분에 사용(StretchBlt)
int W2DY(int Y) { return MulDiv(Y - g_panningY, g_zoomVal, ZOOMBASE); }         // 이미지 부분에 사용(StretchBlt)

int D2W(int R) { return MulDiv(R, ZOOMBASE, g_zoomVal); }                       // Device to WorkSpace(크기 변환하는 함수)
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
        printf("S.cx: %d, S.cy: %d\n", S.cx, S.cy);

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
//      윈도우 영역에서 클립아트는 여기서 모두 작업합니다
//-----------------------------------------------------------------------------
void WINAPI DrawClipArt(HWND hWnd, HDC hDC)
{
    //SIZE S;
    //S.cx = W2DX(g_clipArtRect.right);
    //S.cy = W2DX(g_clipArtRect.bottom);
    //SetRect(&g_clipArtRect, 0, 0, S.cx, S.cy);
    //printf("S.cx: %d, S.cy: %d\n", S.cx, S.cy);

    // 클립아트 위치 조정(텍스트는 GetTextExtentPoint32 함수를 써서 동적으로 SIZE 구조체를 채우던데..)
    //SetRect(&g_clipArtRect, W2DX(g_clipArtRect.left), W2DY(g_clipArtRect.top), W2DX(g_clipArtRect.right), W2DY(g_clipArtRect.bottom));
    printf("%d %d %d %d\n", g_clipArtRect.left, g_clipArtRect.top, g_clipArtRect.right, g_clipArtRect.bottom);
    
    // 클립아트 크기 조절
    SetRect(&g_clipArtRect, W2DX(0), W2DY(0), W2DX(g_clipArtSzX), W2DY(g_clipArtSzY));
    PlayEnhMetaFile(hDC, g_hEnh, &g_clipArtRect);
}



//-----------------------------------------------------------------------------
//      윈도우 영역에서 글꼴, 색상, 크기는 여기서 모두 작업합니다
//-----------------------------------------------------------------------------
void WINAPI DrawTextAll(HWND hWnd, HDC hDC)
{
    RECT  R;
    HFONT hFont, oldFont;

    for (int i = 0; i < g_textCnt; i++)
    {
        SetRect(&R, W2DX(g_textRect[i].left), W2DY(g_textRect[i].top), W2DX(g_textRect[i].right), W2DY(g_textRect[i].bottom));

        hFont = MyCreateFont(W2D(g_textSz[i]), FALSE, FALSE, g_textFontArr[i]); // {2: Bold, 3: Italic}
        oldFont = (HFONT)SelectObject(hDC, hFont);

        SetTextColor(hDC, RGB(g_textColor[i].rgbBlue, g_textColor[i].rgbGreen, g_textColor[i].rgbRed)); //BGR 순으로해야 색이 정확함
        SetBkMode(hDC, TRANSPARENT);

        DrawTextEx(hDC, g_textStrArr[i], -1, &R, DT_VCENTER | DT_WORDBREAK, NULL);      //실제 문자열을 찍는 문장

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

    for (Y = 0; Y < g_PAPERYQTY; Y++)
    {
        for (X = 0; X < g_PAPERXQTY; X++)
        {
            px = X * PAPER_X_SIZE;
            py = Y * PAPER_Y_SIZE;
            Rectangle(hDC, W2DX(px), W2DY(py), W2DX(px + PAPER_X_SIZE), W2DY(py + PAPER_Y_SIZE));
        }
    }
}



//-----------------------------------------------------------------------------
//      모든 화면 그리는 동작
//-----------------------------------------------------------------------------
void WINAPI DrawAll(HWND hWnd, HDC hDC)
{
    DrawStretchBitmap(hDC, g_hImageLoaded, W2DX(0), W2DY(0), W2DX(g_imgSzX), W2DY(g_imgSzY));   // 이미지를 그림
    DrawClipArt(hWnd, hDC);                                                                     // 클립아트를 그림                                                                                          
    DrawTextAll(hWnd, hDC);                                                                     // 텍스트를 그림

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
    DrawCircle(hdc, W2DX(g_imgSzX), W2DY(g_imgSzY), RADIUS);            // 우측 하단 꼭짓점
}



//-----------------------------------------------------------------------------
//      크기 조정중인 안내선을 그림
//-----------------------------------------------------------------------------
void WINAPI DrawSizeInfoLine(HWND hWnd)
{
    HDC hdc = GetDC(hWnd);

    SetROP2(hdc, R2_XORPEN);                            // GDI함수가 화면에 출력을 내보낼 때 화면에 이미 출력되어 있는 그림과 새로 그려지는 그림과의 관계를 정의하는 함수
    SelectObject(hdc, GetStockObject(WHITE_PEN));
    DrawImgBoundaryLine(hdc);                           // 이미지 경계선
    DrawImgVertex(hdc);                                 // 이미지 꼭짓점
    InvalidateRect(hWnd, NULL, FALSE);                  // 적용하면 텍스트 잔상처리가 해결되지만 반짝거리고 안하면 텍스트 잔상처리가 해결안됨
    ReleaseDC(hWnd, hdc);
}



//-----------------------------------------------------------------------------
//      이미지 크기를 조정할 위치정보를 리턴함
//-----------------------------------------------------------------------------
int WINAPI GetDragingMode(HWND hWnd, POINT P)
{
    int  i, DragingMode = NO_SELECTED;
    HDC  hdc;
    RECT R;
    static const int textMode[] = { IN_TEXT_1, IN_TEXT_2, IN_TEXT_3 };

    hdc = GetDC(hWnd);

    // -------텍스트 부분-------
    for (i = 0; i < g_textCnt; i++)
    {
        SetRect(&R, W2DX(g_textRect[i].left), W2DY(g_textRect[i].top), W2DX(g_textRect[i].right), W2DY(g_textRect[i].bottom));
        if (PtInRect(&R, P)) { DragingMode = textMode[i]; goto ProcExit; }
    }

    // -------이미지 부분-------
    SetRect(&R, W2DX(g_imgSzX) - CL, W2DY(g_imgSzY) - CL, W2DX(g_imgSzX) + CL, W2DY(g_imgSzY) + CL);
    if (PtInRect(&R, P)) { DragingMode = ADJUST_DIAGONAL; goto ProcExit; }

    SetRect(&R, W2DX(g_imgSzX) - CL, W2DY(0), W2DX(g_imgSzX) + CL, W2DY(g_imgSzY) - CL);
    if (PtInRect(&R, P)) { DragingMode = ADJUST_WIDTH; goto ProcExit; }

    SetRect(&R, W2DX(0), W2DY(g_imgSzY) - CL, W2DX(g_imgSzX) - CL, W2DY(g_imgSzY) + CL);
    if (PtInRect(&R, P)) { DragingMode = ADJUST_HEIGHT; goto ProcExit; }

    // -------클립아트 부분------
    // 클립아트 위치를 움직이는 부분
    SetRect(&R, W2DX(g_clipArtRect.left), W2DY(g_clipArtRect.top), W2DX(g_clipArtRect.right), W2DY(g_clipArtRect.bottom));
    if (PtInRect(&R, P)) { DragingMode = IN_CLIPART; goto ProcExit; }

    // 클립아트 사이즈를 조정하는 부분
    SetRect(&R, W2DX(g_clipArtSzX) - CL, W2DY(g_clipArtSzY) - CL, W2DX(g_clipArtSzX) + CL, W2DY(g_clipArtSzY) + CL);
    if (PtInRect(&R, P)) { DragingMode = LINE_CLIPART; goto ProcExit; }

ProcExit:
    ReleaseDC(hWnd, hdc);
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
    case ADJUST_WIDTH:      g_imgSzX += DX;                         break;
    case ADJUST_HEIGHT:     g_imgSzY += DY;                         break;
    case ADJUST_DIAGONAL:   g_imgSzX += DX; g_imgSzY += DY;         break;
    case IN_TEXT_1:         i = 0;                                  goto AdjText;           // 첫 번째 텍스트
    case IN_TEXT_2:         i = 1;                                  goto AdjText;           // 두 번째 텍스트
    case IN_TEXT_3:         i = 2;                                  goto AdjText;           // 세 번째 텍스트
    case IN_CLIPART:                                                goto AdjText;           // 클립아트 좌표 이동
    case LINE_CLIPART:      g_clipArtSzX += DX; g_clipArtSzY += DY; break;                  // 클립아트 크기 조정

    AdjText:
        OffsetRect(&g_textRect[i], DX, DY);
        OffsetRect(&g_clipArtRect, DX, DY);
    }

    DrawSizeInfoLine(hWnd);
}



//-----------------------------------------------------------------------------
//      모든 화면 그리는 동작
//-----------------------------------------------------------------------------
void MousePanning(HWND hWnd, UINT Msg, WPARAM wPrm, LPARAM lPrm)
{
    static int DragingMode = NO_SELECTED;
    static POINT oldP;
    static int currZoom = 4;
    static const int zoomTable[] = { 2000, 1000, 500, 250, 125, 63, 31, 15 };

    switch (Msg)
    {
    case WM_LBUTTONDOWN:
        oldP.x = LoInt16(lPrm);
        oldP.y = HiInt16(lPrm);

        if ((DragingMode = GetDragingMode(hWnd, oldP)) == NO_SELECTED) break;
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
            if (currZoom < COUNTOF(zoomTable) - 1)
            {
                g_zoomVal = zoomTable[++currZoom];
                InvalidateRect(hWnd, NULL, TRUE);
            }
        }
        break;
    }
}



//----------------------------------------------------------------------------
//      파일을 열어 이미지 또는 클립아트를 선택합니다.
//----------------------------------------------------------------------------
BOOL WINAPI OpenImage(HWND hWnd, LPSTR Buff, int BuffSize, LPCSTR Title, LPCSTR Filter)
{
    OPENFILENAME ofn;
    BOOL rv; // return value

    ZeroMemory(&ofn, sizeof(OPENFILENAME));
    ofn.lStructSize = sizeof(OPENFILENAME);
    ofn.hwndOwner = hWnd;
    ofn.lpstrTitle = Title;
    ofn.lpstrFilter = Filter;
    ofn.lpstrFile = Buff;
    ofn.nMaxFile = BuffSize;

    // 파일열기 대화상자를 열고, 선택된 파일의 이름을 에디트 박스로 복사
    if ((rv = GetOpenFileName(&ofn)) != 0)
    {
        if (g_hEnh) DeleteEnhMetaFile(g_hEnh);
        g_hEnh = ReadMeta(Buff);

        if (g_hEnh) InvalidateRect(hWnd, NULL, TRUE);
    }

    return rv;
}



//-----------------------------------------------------------------------------
//      사용자로부터 입력받은 A4 용지 가로/세로 장 수를 처리합니다.
//-----------------------------------------------------------------------------
void WINAPI SaveA4Info(HWND hWnd)
{
    if (GetDlgItemInt(hWnd, IDC_A4_WIDTH_EDIT, NULL, FALSE) == FALSE || GetDlgItemInt(hWnd, IDC_A4_HEIGHT_EDIT, NULL, FALSE) == FALSE)
        MessageBox(hWnd, "입력 칸에 정수를 입력해 주세요.", "알림", MB_OK);
    else
    {
        g_PAPERXQTY = GetDlgItemInt(hWnd, IDC_A4_WIDTH_EDIT, NULL, FALSE);
        g_PAPERYQTY = GetDlgItemInt(hWnd, IDC_A4_HEIGHT_EDIT, NULL, FALSE);
        g_A4Status = TRUE;      // 사용자가 정상적으로 A4 장 수를 입력한 경우(while문을 탈출하기 위함)
    }
}



//-----------------------------------------------------------------------------
//      방향키를 입력받았을 때 전체 화면이 움직이는 함수
//-----------------------------------------------------------------------------
void WINAPI KeyProc(HWND hWnd, int key)
{
    switch (key)
    {
    case VK_LEFT:
        g_panningX += 100;
        InvalidateRect(hWnd, NULL, TRUE);       // 세 번째 인자가 FALSE이면 잔상이 남음 
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

    g_zoomVal = prtResX * 1000 / PAPER_X_SIZE;

    for (Y = 0; Y < g_PAPERYQTY; Y++)
    {
        for (X = 0; X < g_PAPERXQTY; X++)
        {
            StartPage(hPrnDC);
            g_panningX = X * PAPER_X_SIZE;
            g_panningY = Y * PAPER_Y_SIZE;
            DrawAll(hWnd, hPrnDC); // 이미지, 텍스트, 클립아트를 출력물에 그림
            EndPage(hPrnDC);
        }
    }
    EndDoc(hPrnDC);

ProcExit:
    if (hPrnDC) DeleteDC(hPrnDC);
    g_zoomVal   = orgZoom;
    g_panningX  = orgPanX;
    g_panningY  = orgPanY;
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
        //g_hButtonOpenFileDialog = GetDlgItem(hWnd, IDC_OPEN_FILE_BTN); // 주석처리해도 작동은 잘됨(이유를 모르겠음)
        g_hEditFileToBeOpened   = GetDlgItem(hWnd, IDC_IMG_ROUTE_EDIT);
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
        CreateComboBox(hWnd); // 글꼴, 사이즈 콤보박스 생성
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
            SaveTextInfo(hWnd); // 문자열, 글꼴, 사이즈, 색상 정보 저장
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
    if (g_textCnt < 3) // 텍스트를 3개까지 추가 가능
    {
        if (DialogBox(hInst, MAKEINTRESOURCE(IDD_ADD_TEXT_DLG), hWnd, TextDialogBoxProc) == IDOK) g_textCnt++;
        InvalidateRect(hWnd, NULL, TRUE); // WndProc 내 WM_PAINT 메시지가 다시 발생한다.
    }
    else
        MessageBox(hWnd, "텍스는 3개까지 추가가 가능합니다.", "알림", MB_OK);
}



//-----------------------------------------------------------------------------
//      '클립아트 추가' 메뉴를 클릭했을 때 실행하는 함수
//-----------------------------------------------------------------------------
void WINAPI AddClipArtProc(HWND hWnd)
{
    char szFileName[MAX_PATH];
    szFileName[0] = 0;

    if (OpenImage(hWnd, szFileName, sizeof(szFileName), "클립아트(WMF 또는 EMF) 파일을 선택하세요", "Meta File\0*.?MF\0") == FALSE) return;
    SetWindowText(g_hEditFileToBeOpened, szFileName);
    lstrcpy(g_clipArtRoute, szFileName);    // 이미지를 오픈 할때와는 다름(이미지는 editbox에 경로가 넣어졌기 때문에)

    InvalidateRect(hWnd, NULL, TRUE);
}



//-----------------------------------------------------------------------------
//      정보 대화 상자의 메시지 처리기입니다.
//-----------------------------------------------------------------------------
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
        return (INT_PTR)TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
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
    {
        while (!g_A4Status)
            DialogBox(hInst, MAKEINTRESOURCE(IDD_INIT_DLG), hWnd, A4DialogBoxProc); // 사용자로부터 A4 장 수 입력받음

        lstrcpy(g_imgRoute, "girl.bmp"); LoadBmpImage();    // 테스트할 때 이미지 열기 귀찮아서 한가인 사진으로 설정해놓음
        g_hPenPapaer = CreatePen(PS_SOLID, 1, RGB(109, 202, 185));
    }
        return 0;

    case WM_DESTROY:        //윈도우가 파기될 때
        if (g_hImageLoaded) DeleteObject(g_hImageLoaded);
        if (g_hPenPapaer)   DeleteObject(g_hPenPapaer);
        if (g_hEnh)         DeleteEnhMetaFile(g_hEnh);
        PostQuitMessage(0); //GetMessage()의 리턴을 FALSE로 만들어 종료하게 함
        return 0;

    case WM_PAINT:          //화면을 그려야 할 이유가 생겼을 떄
        PAINTSTRUCT PS;
        BeginPaint(hWnd, &PS);
        DrawAll(hWnd, PS.hdc);          // 이미지, 텍스트, 클립아트를 그림
        DrawPaper(hWnd, PS.hdc);        // 미리보기 선을 그림
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

        case IDM_ABOUT:
            DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About); break;

        case IDM_EXIT:
            DestroyWindow(hWnd); break;
        }
        return 0;
  
    case WM_SETCURSOR:
        GetCursorPos(&P);
        ScreenToClient(hWnd, &P); // 노트북 화면이 기준이 아니라 작업 영역을 기준으로 함
        switch (GetDragingMode(hWnd, P))
        {
        case ADJUST_WIDTH:      SetCursor(LoadCursor(NULL, IDC_SIZEWE));      return TRUE;
        case ADJUST_HEIGHT:     SetCursor(LoadCursor(NULL, IDC_SIZENS));      return TRUE;
        case ADJUST_DIAGONAL:   SetCursor(LoadCursor(NULL, IDC_SIZENWSE));    return TRUE;
        case IN_TEXT_1:         SetCursor(LoadCursor(NULL, IDC_HAND));        return TRUE;
        case IN_TEXT_2:         SetCursor(LoadCursor(NULL, IDC_HAND));        return TRUE;
        case IN_TEXT_3:         SetCursor(LoadCursor(NULL, IDC_HAND));        return TRUE;
        case IN_CLIPART:        SetCursor(LoadCursor(NULL, IDC_HAND));        return TRUE;
        case LINE_CLIPART:      SetCursor(LoadCursor(NULL, IDC_SIZENWSE));    return TRUE;
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