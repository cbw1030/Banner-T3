# 현수막 제작 프로그램
A4 용지를 이어붙여 원하는 크기의 현수막을 제작하기 위함
  
## :smile: Ahthor
> [조재희](https://github.com/cbw1030)

## :man_juggling: 구현기능
- [x] 사용자로부터 원하는 A4 장 수를 입력받음
- [x] 인쇄 전 미리보기 기능(사용자로부터 입력받은 값을 가로/세로 그리드로 나타냄)
- [x] 마우스 휠을 통해 전체화면 확대 및 축소 가능
- [x] 방향키를 이용해 그리드 위치 이동 가능
- [x] 텍스트 추가 기능(폰트, 크기, 색상 적용 및 원하는 위치 이동 가능)
- [x] 클립아트(wmf 또는 emf 파일) 추가 가능(늘이기 또는 줄이기 가능)
- [x] 이미지(bmp) 추가 가능(늘이기 또는 줄이기 가능)

## :mag: 사용기술
> WIN32 API 

## :worried: 개발에 어려웠던 점
> - 대부분 예제가 MFC 또는 C#이어서 예제나 도큐먼트를 찾기 어려웠음(WMF, EMF, OLE)
> - 이미지, 텍스트를 인쇄할 때 같은 로직을 적용해야 하는데 중반부까지 서로 다른 로직을 적용하다가 결국 출력할 때 사이즈가 맞지 않는 문제가 발생했다. 이후, 작업영역과 디바이스 좌표를 변환해주는 함수를 고안하여 해결할 수 있었음
> - 충분한 설계를 하지 않고 무작정 하드코딩으로 작업을 했기 때문에 수정사항이 생겼을 때, 완전 갈아 엎어야 하는 경우가 몇 번 있었음. 이를 통해 설계 단계의 중요성을 알게 되었음

## :worried: 미처리 사항
>- A4의 오버랩 부분을 적용하지 못함
>- bmp 이미지를 한 개밖에 열지 못함
>- OLE(Object Linked Embedded) 적용하지 못함
>- 클립아트를 이동시키지 못함(텍스트는 GetTextExtentPoint32 함수(동적으로 텍스트의 길이를 반환)를 사용하면 되지만, 클립아트는 위와 같은 함수가 존재하지 않아 이동을 못 시킴)

## :open_mouth: 첨부자료
>- MSDN Reference
>- http://www.soen.kr WINAPI 입문 강좌와 Reference 

## :page_facing_up: Notes
> - Visual Studio 2019 사용
> - 인턴 개인 프로젝트
