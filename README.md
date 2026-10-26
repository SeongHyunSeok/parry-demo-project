# Soul-Like Simulator

적의 공격 타이밍을 읽고 방패로 받아친 뒤 반격하는 손맛에 집중한 **3D 액션 전투 시뮬레이터**입니다. 이 게임은 C++과 DirectX 12로 프레임워크를 구성했습니다.
## 게임 소개

플레이어는 이동과 구르기로 거리를 조절하고, 방패로 공격을 막거나 한손 무기로 패링할 수 있습니다.

1. 적의 자세와 공격 예고를 확인합니다.
2. 구르기, 방어 또는 패링으로 대응합니다.
3. 패링에 성공하면 경직된 적에게 강한 반격이나 처형을 시도합니다.

## 핵심 기능

### 패링과 처형

<img width="270" height="456" alt="스크린샷 2026-10-26 142539" src="https://github.com/user-attachments/assets/713bab1b-1622-42fe-88dd-87a5c023ba78" /> <img width="270" height="456" alt="스크린샷 2026-10-26 142607" src="https://github.com/user-attachments/assets/c7d891ef-c1aa-4dc4-a70a-051e202fc387" /> <img width="270" height="456" alt="스크린샷 2026-10-26 142701" src="https://github.com/user-attachments/assets/e5d236d0-c4c5-4b9d-a676-146a0b54e991" />




패링은 플레이어와 적의 거리, 적 공격 애니메이션의 타격 구간, 플레이어 패링 유효 구간을 함께 검사합니다.  
성공하면 적이 경직되고 검 충돌음과 섬광·스파크가 재생되며, 반격과 처형 기회가 주어집니다.

### 기본 설정과 Combat 모드 설정

<img width="1276" height="999" alt="image" src="https://github.com/user-attachments/assets/fb27d31f-39b9-4e37-97a3-419d56ba3ca2" />

플레이어에 대한 피격판정과 패링판정에 도움이 되는 시각적 효과를 설정 가능합니다.  
전투에서 사용되는 체력, 스테미나, 공격력같은 수치들과 패링 유효 프레임, 구르기 유효 프레임을 설정가능합니다.

### 적 패턴과 Attack Delay 편집

<img width="769" height="991" alt="image" src="https://github.com/user-attachments/assets/341af33d-d66e-42b7-bdf8-2e7103dc95c7" />

ImGui 편집기에서 적의 패턴들을 클릭 또는 드래그해 적의 행동을 구성할 수 있습니다.  
기본적으로 각 행동별로 가중치가 존재 이걸 토대로 AI가 작동합니다. 같은 패턴을 중복해서 넣으면 그 패턴의 가중치가 증가합니다.  
Fixed order옵션을 켜면 등록한 순서대로 실행해 특정 패턴을 반복합니다.  
Attack Delay 편집을 통해 적의 공격별로 공격 직전에 딜레이를 최대 60프레임까지 설정할 수 있습니다.  

## 기술 스택

| 구분 | 기술 |
| --- | --- |
| Language | C++20, Win32 |
| Graphics | DirectX 12, HLSL 5.1 |
| Rendering | Shadow Mapping, SSAO, Normal Mapping, GPU Skinning, Billboard Particle |
| Physics | Jolt Physics, Root Motion 연동 |
| Architecture | ECS 스타일 컴포넌트, Handle·DenseTable, FSM, Event Dispatch |
| Tools | Dear ImGui, miniaudio |

## 구조

플레이어와 적을 하나의 큰 클래스에 넣지 않고 Mesh, Animation, Physics, State Machine과 Equipment 컴포넌트로 조립했습니다.  
컴포넌트는 타입별 `DenseTable`에 저장하고 Handle ID로 안전하게 접근합니다.

## 조작법

시작 화면에서 `Space`를 누르면 데모가 시작됩니다.

| 입력 | 동작 |
| --- | --- |
| `W A S D` | 이동 |
| 이동 중 `Space` | 구르기 |
| 마우스 왼쪽 / 오른쪽 | 공격 / 패링 |
| `Shift` | 방패 들기 |
| 마우스 가운데 | 타깃 고정 |
| `1` / `2` | 설정 UI / 적 AI 전환 |
| `[` / `]` | 게임 속도 조절 |
| Combat Mode `Esc` / `T` | 일시 정지 / 재시작 |
| `End` | 종료 |

## 빌드

Windows 10 이상, DirectX 12 지원 GPU, Windows SDK와 C++ 데스크톱 개발 워크로드가 설치된 Visual Studio가 필요합니다. 프로젝트는 MSVC `v145`를 사용합니다.

```powershell
git clone https://github.com/SeongHyunSeok/parry-demo-project.git
```

## 실행법

**ParryDemo.zip**을 받아 압축을 풀고 **ParryDemoProject.exe** 실행

## 사용 범위

- Jolt Physics는 충돌 계산, Dear ImGui는 개발 UI, miniaudio는 오디오 출력에 사용했습니다.
- DirectX 12 렌더링 연결, ECS 스타일 저장소, 전투 상태 머신, 패링 판정, 애니메이션·물리 연동, 패턴·Delay 편집 로직과 파티클 운용은 프로젝트에서 구현했습니다.
- 패턴과 Delay 설정은 현재 실행 세션 동안만 유지됩니다.
