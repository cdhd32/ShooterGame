# DirectX 12: Root Signature를 공유하는 PSO 사례

Scene 데이터는 프레임마다 한 번 갱신하고, 같은 Root Signature를 쓰는 여러 PSO가 그 바인딩을 재사용하는 것이 일반적인 구조다.

PSO는 셰이더, Blend, Depth, Rasterizer, 입력 레이아웃 같은 렌더링 상태를 바꾸고, Root Signature는 셰이더가 어떤 리소스 슬롯을 읽는지 정의한다.

## 핵심 원칙

같은 Root Signature를 유지한 채 PSO만 바꾸면 Scene CBV를 다시 바인딩할 필요가 없다.

반대로 다른 Root Signature를 설정하면 이전 Root 파라미터 바인딩은 stale 상태가 되므로 Draw 전에 새 레이아웃의 바인딩을 다시 설정해야 한다.

## 대표 사례

| 렌더링 묶음 | PSO가 달라지는 부분 | 공용 Root Signature 슬롯 예시 |
| --- | --- | --- |
| 불투명 / 알파 테스트 / 투명 메시 | Blend, DepthWrite, Pixel Shader | Scene b0, Object b1, Material/Texture b2 |
| 일반 렌더링 / 그림자 맵 / Depth pre-pass | Depth 상태, CullMode, Pixel Shader | Scene, Object, Material |
| 일반 메시 / Grid / XYZ Gizmo | 입력 레이아웃, Topology, Shader | Scene b0 중심의 공용 레이아웃 |
| 스킨 메시 / 정적 메시 | Vertex Shader, 정점 형식 | Scene, Object, Material, Bone 슬롯 |
| HDR 톤매핑 / 팔레트 / 화면 합성 | Pixel Shader, Render Target, Blend | 화면 텍스처와 공용 상수 |

## ShooterGame 권장 레이아웃

```text
Root parameter 0 : SceneData        b0  (프레임 공용)
Root parameter 1 : Transform        b1  (객체별)
Root parameter 2 : Material/Texture b2  (재질별)

PSO A : PMD 모델
PSO B : Grid와 XYZ Gizmo
PSO C : 바닥과 벽
PSO D : 알파 블렌드 이펙트
```

## 프레임 렌더링 예시

```cpp
cmdList->SetGraphicsRootSignature(
    _commonRootSignature.Get());

_dx12->SetScene(
    _camera.View(),
    _camera.position);  // Scene b0: 프레임당 한 번

cmdList->SetPipelineState(_pmdPipeline.Get());
_pmdActor->Draw();

cmdList->SetPipelineState(_linePipeline.Get());
_lineRenderer->Draw();  // Scene 재바인딩 불필요
```

## 실제 공개 사례

Microsoft DirectX-Graphics-Samples의 D3D12 HDR 샘플은 한 Root Signature와 루트 상수 및 SRV 테이블을 설정한 뒤, Gradient PSO, Palette PSO, Present PSO를 전환해 여러 종류의 화면 요소를 그린다.

Microsoft 문서는 같은 Root Signature를 공유하는 PSO 그룹을 이상적인 구조로 설명한다.

## 참고 자료

- [Microsoft Learn - Root Signatures Overview](https://learn.microsoft.com/en-us/windows/win32/direct3d12/root-signatures-overview)
- [Microsoft Learn - Using a Root Signature](https://learn.microsoft.com/en-us/windows/win32/direct3d12/using-a-root-signature)
- [Microsoft DirectX-Graphics-Samples - D3D12 HDR](https://github.com/microsoft/DirectX-Graphics-Samples/blob/master/Samples/Desktop/D3D12HDR/src/D3D12HDR.cpp)
- [Microsoft DirectXTK12 - PSOs, Shaders, and Signatures](https://github.com/microsoft/DirectXTK12/wiki/PSOs,-Shaders,-and-Signatures)
