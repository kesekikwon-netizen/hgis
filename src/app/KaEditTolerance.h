#pragma once

// Screen-pixel hit tolerances of the drawing, select and vertex tools, kept in one
// place so pen or touch use can be tuned without hunting through every tool.
namespace KaEditTolerance {

inline constexpr int kSketchVertexPx = 10;    // a new sketch point on top of an earlier one
inline constexpr int kFeaturePickPx = 10;     // clicking a line or point to select it
inline constexpr int kVertexGrabPx = 20;      // pressing a vertex handle to drag it
inline constexpr int kVertexMenuPx = 24;      // right-click near a vertex offers 점삭제
inline constexpr int kVertexMenuFarPx = 28;   // standalone vertex menu, looser fallback
inline constexpr int kSegmentPx = 16;         // right-click near an edge offers 점추가
inline constexpr int kClickSlopPx = 4;        // press and release closer than this is a click
inline constexpr int kEasyDrawDwellMs = 700;  // easy draw: resting on an edge adds that point

}  // namespace KaEditTolerance
