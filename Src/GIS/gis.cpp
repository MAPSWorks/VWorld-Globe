
#include "stdafx.h"
#include "gis.h"
#include "quadtree.h"

using namespace gis;

// 레벨당 위경도 각도
// index = level
static const double deg[16] = {
	36.f  // 0 level
	, 36.f / 2.f // 1 level
	, 36.f / 2.f / 2.f // 2 level
	, 36.f / 2.f / 2.f / 2.f // 3 level
	, 36.f / 2.f / 2.f / 2.f / 2.f // 4 level
	, 36.f / 2.f / 2.f / 2.f / 2.f / 2.f // 5 level
	, 36.f / 2.f / 2.f / 2.f / 2.f / 2.f / 2.f // 6 level
	, 36.f / 2.f / 2.f / 2.f / 2.f / 2.f / 2.f / 2.f // 7 level
	, 36.f / 2.f / 2.f / 2.f / 2.f / 2.f / 2.f / 2.f / 2.f // 8 level
	, 36.f / 2.f / 2.f / 2.f / 2.f / 2.f / 2.f / 2.f / 2.f / 2.f // 9 level
	, 36.f / 2.f / 2.f / 2.f / 2.f / 2.f / 2.f / 2.f / 2.f / 2.f / 2.f // 10 level
	, 36.f / 2.f / 2.f / 2.f / 2.f / 2.f / 2.f / 2.f / 2.f / 2.f / 2.f / 2.f // 11 level
	, 36.f / 2.f / 2.f / 2.f / 2.f / 2.f / 2.f / 2.f / 2.f / 2.f / 2.f / 2.f / 2.f // 12 level
	, 36.f / 2.f / 2.f / 2.f / 2.f / 2.f / 2.f / 2.f / 2.f / 2.f / 2.f / 2.f / 2.f / 2.f // 13 level
	, 36.f / 2.f / 2.f / 2.f / 2.f / 2.f / 2.f / 2.f / 2.f / 2.f / 2.f / 2.f / 2.f / 2.f / 2.f // 14 level
	, 36.f / 2.f / 2.f / 2.f / 2.f / 2.f / 2.f / 2.f / 2.f / 2.f / 2.f / 2.f / 2.f / 2.f / 2.f / 2.f // 15 level
};



// return Tile Left-Top WGS84 Location
// xLoc : col, 경도
// yLoc : row, 위도
//
// 0 level, row=5, col=10
//	- latitude : 36 degree / row 1
//	- longitude : 36 degrre / col 1
//
// return: .x = longitude (경도)
//		   .y = latitude (위도)
Vector2d gis::TileLoc2WGS84(const int level, const int xLoc, const int yLoc)
{
	Vector2d lonLat;
	lonLat.x = deg[level] * xLoc - 180.f;
	lonLat.y = deg[level] * yLoc - 90.f;
	return lonLat;
}


Vector2d gis::TileLoc2WGS84(const sTileLoc &loc)
{
	return TileLoc2WGS84(loc.level, loc.xLoc, loc.yLoc);
}


sTileLoc gis::WGS842TileLoc(const Vector2d &lonLat)
{
	assert(0);
	return {};
}


// Convert 3D Position to longitude, raditude
// return: .x = longitude (경도)
//		   .y = latitude (위도)
//
// leftBottomLonLat, rightTopLonLat:
//   +--------* (Right Top Longitude Latitude)
//   |        |
//   |        |
//   |        |
//   *--------+ 
//	(Left Bottom Longitude Latitude)
//
// rect: VWorld 3D Coordinate system
//       Ground X-Z Plane Rect
//
//  /\ Z 
//  |
//  | 
//  |
//  ---------> X 
//  Ground = X-Z Plane
//
Vector2d gis::Pos2WGS84(const Vector3 &pos
	, const Vector2d &leftBottomLonLat, const Vector2d &rightTopLonLat
	, const sRectf &rect)
{
	Vector3 p = Vector3(pos.x - rect.left, pos.y, pos.z - rect.top);
	p.x /= rect.Width();
	p.z /= rect.Height();

	Vector2d ret;
	ret.x = common::lerp(leftBottomLonLat.x, rightTopLonLat.x, (double)p.x);
	ret.y = common::lerp(leftBottomLonLat.y, rightTopLonLat.y, (double)p.z);
	return ret;
}


// Convert 3D Global Pos to longitude, latitude
Vector2d gis::Pos2WGS84(const Vector3 &globalPos)
{
	const double size = 1 << 16;

	Vector2d ret;
	ret.x = (double)globalPos.x * (36.f / size) - 180.f;
	ret.y = (double)globalPos.z * (36.f / size) - 90.f;
	return ret;
}


// Convert longitude, latitude to 3D Position (Global Position)
Vector3 gis::WGS842Pos(const Vector2d &lonLat)
{
	const double size = 1 << 16;
	const double x = (lonLat.x + 180.f) * (size / 36.f);
	const double z = (lonLat.y + 90.f) * (size / 36.f);
	return Vector3((float)x, 0, (float)z);
}


Vector3 gis::GetRelationPos(const Vector3 &globalPos
	, const float scale //= 1.f
)
{
	const float size = (1 << (16 - g_offsetLv)) * scale;
	const int xLoc = g_offsetXLoc;
	const int yLoc = g_offsetYLoc;
	const float offsetX = size * xLoc;
	const float offsetY = size * yLoc;
	return Vector3(globalPos.x - offsetX, globalPos.y, globalPos.z - offsetY);
}


Vector3 gis::GetGlobalPos(const Vector3 &relPos
	, const float scale //= 1.f
)
{
	const float size = (1 << (16 - g_offsetLv)) * scale;
	const int xLoc = g_offsetXLoc;
	const int yLoc = g_offsetYLoc;
	const float offsetX = size * xLoc;
	const float offsetY = size * yLoc;
	return Vector3(relPos.x + offsetX, relPos.y, relPos.z + offsetY);
}


// meter -> 3D unit
// 지구 적도 둘레, 40030000.f,  40,030 km
// 경도축으로 10등분 한 크기를 (1 << cQuadTree<int>::MAX_LEVEL) 로 3D로 출력한다.
float gis::Meter23DUnit(const float meter)
{
	static const float rate = (float)(1 << cQuadTree<int>::MAX_LEVEL) / (40030000.f * 0.1f);
	return meter * rate;
}


// return Normal Vector in Globe Sphere Space Coordinate System
//
//  Y axis (lonLat = 0,90)
//  /\       
//   |       / Z axis  (lonLat = 180,0)
//   |     /
//   |   /
//   | /
//  --------------> X axis (lonLat = 90, 0)
//
Vector3 gis::GetNormal(const Vector2d &lonLat)
{
	// convert radian
	const double lon = ANGLE2RAD(lonLat.x);
	const double lat = ANGLE2RAD(lonLat.y);

	Vector3 longitude((float)sin(lon), 0, (float)-cos(lon));
	longitude.Normalize();
	Vector3 normal = longitude * (float)cos(lat)
		+ Vector3(0, 1, 0) * (float)sin(lat);
	normal.Normalize();
	return normal;
}


// return Globe Sphere Space Coordinate System
Vector3 gis::WGS842PosSphere(const Vector2d &lonLat)
{
	return GetNormal(lonLat) * g_globeRadius;
}
