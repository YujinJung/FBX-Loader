# DirectX Animation

<br>

## FrameResource
### struct
#### SkinnedConstants
- BoneTransformation

#### SkinnedVertex
- Pos,  Normal, TexC, TangentU, BoneWeights
- BoneIndices[4]

---

## SkinnedData
### struct
#### Keyframe
- TimePos
- Translation, Scale, RotationQuat

#### BoneAnimation
- Keyframes (vector)
- Keyframes front, end TimePos 얻는 함수
- Interpolate 함수

#### AnimationClip
- BoneAnimations (vector)
- BoneAnimation 중 가장 빠른 그리고 가장 느린 시간
- Interpolate 함수

### Class SkinnedData
- GetFinalTransforms (function)
    - Bone 갯수
    - toParentTransforms (부모 뼈 좌표계로의 변환행렬)
    - toRootTransforms 앞에부터 하나씩 채워나감
        - 앞에부터 toParentTransforms를 곱해가면 toRootTransforms가 나온다.
    - 

---

## LoadM3d
### Function
#### LoadM3d
- Material, Vertices, Triangles, Bones, AnimationClips 갯수 받아오기
- 

---
---

# FBX Animation