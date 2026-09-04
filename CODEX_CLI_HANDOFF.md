# Ration Run — Codex CLI 인수인계

작성 시각: 2026-09-04 KST
작업 브랜치: `release-1.44`

## 이번 작업에서 완료한 내용

- 사격 무반응 원인을 확인했다. `Model/cylinder_512.pmd`의 메시 중심은 로컬 Y 약 `15.0`인데, 드론의 논리 위치는 Y=`1.0`·스케일은 `0.55`였다. 따라서 보이는 메시 중심은 Y=`9.25`지만 히트스캔 충돌구는 Y=`1.0`을 사용하고 있었다.
- `Application.cpp`에 `GetEnemyRenderPosition()`을 추가했다. 렌더 변환의 Y를 `15.0 * 0.55 = 8.25`만큼 내리므로 화면상 드론 중심과 논리 충돌 중심이 모두 Y=`1.0`이 된다.
- Debug 전용 로그를 추가했다. 시작/생성/클릭/사격/명중/빗나감/피해/승패/재시작/초당 상태를 콘솔과 Visual Studio Output에 함께 출력한다. Release에서는 no-op다.
- 창 내부 HUD를 추가했다. Win32 `STATIC` 컨트롤만 사용하므로 추가 셰이더·텍스처·외부 DLL이 없다.
  - 좌상단: HP, SCORE, TIME, 조작 안내
  - 중앙: 고정 `+` 텍스트 조준점
  - 승리/패배: 결과와 `PRESS R TO RESTART`
- `ReleasePackage/README.txt`를 화면 HUD 기준으로 갱신했다.
- 모든 이번 작업의 텍스트 파일은 CRLF로 정규화했다. 이후 수정도 CRLF를 유지할 것.

## 검증 근거

### Debug x64

빌드 명령:

```powershell
& 'C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe' .\ShooterGame.vcxproj /m /p:Configuration=Debug /p:Platform=x64 /p:BuildProjectReferences=false /verbosity:minimal
```

최신 Debug 빌드는 성공했다. 전체 솔루션의 Debug 빌드는 DirectXTex의 `d3dx12.h` include 경로 문제로 실패할 수 있으므로, 현 시점에서는 위의 게임 프로젝트 단독 빌드를 사용한다.

사격 보정 전 실행 로그:

```text
[SPAWN] ... logicalCenter=(-18.00, 1.00, 9.86) visibleCenterY=9.25 centerDeltaY=8.25 ...
```

보정 후 실행 로그:

```text
[SPAWN] ... renderPivotY=-7.25 visibleCenterY=1.00 centerDeltaY=0.00 ...
[STATE] hp=100 score=0 time=89.9 activeEnemies=2 shotCooldown=0.00
```

### Release x64 및 제출물

빌드 명령:

```powershell
& 'C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe' .\ShooterGame.vcxproj /m /p:Configuration=Release /p:Platform=x64 /p:BuildProjectReferences=false /verbosity:minimal
```

- `ReleasePackage\ShooterGame.exe`를 `ReleasePackage`를 작업 폴더로 하여 5초간 독립 실행했고 정상 유지됐다.
- `ReleasePackage` 압축 해제 크기: **466,761 bytes**
- 제출 한도: **1,474,560 bytes**
- 남은 여유: **1,007,799 bytes**
- 최종 ZIP: `RationRun_1.44MB.zip` (232,583 bytes)

## 다음에 할 일

1. Debug 실행을 Visual Studio 또는 `x64\Debug\ShooterGame.exe`로 시작할 때 작업 폴더를 반드시 프로젝트 루트(`C:\Workplace\ShooterGame`)로 둔다.
2. 화면의 `+`에 드론을 맞춘 뒤 좌클릭한다. 콘솔/Output에서 `[INPUT]`, `[SHOT]`, `[HIT]`를 확인한다. `[MISS]`의 `logicalRayDistance`와 `visibleRayDistance`가 모두 의미 있는 수치로 출력된다.
3. 실제 플레이로 `GAME OVER`와 `YOU SURVIVED!` 텍스트가 DirectX 화면 위에 남는지 확인한다. 두 결과 모두 `R`로 재시작되는지 확인한다.
4. 제출 직전에는 `ReleasePackage`에서 다시 독립 실행한 뒤 `RationRun_1.44MB.zip`을 업로드한다.

## 주의 사항

- `PMDActor.cpp`가 `git status`에서 수정으로 보일 수 있지만, 이 작업 시작 시점부터 `git diff -- PMDActor.cpp`는 내용 차이를 보이지 않았다. 강제 reset/checkout으로 지우지 않는다.
- 기존 `Dx12Wrapper.cpp`, `PMDActor.cpp`, Basic 셰이더의 size/sign 변환 및 셰이더 truncation 경고는 남아 있다. 이번 사격/HUD 변경에서 새 오류는 없었고, 마감 직전에는 범위를 넓혀 정리하지 않는다.
- `AGENTS.md`는 로컬 규칙 파일이며 Git ignore 대상이다. 직접 수정·빌드·검증·패키징이 이번 게임잼에 허용되고, 코드·셰이더·문서 텍스트는 CRLF여야 한다.
