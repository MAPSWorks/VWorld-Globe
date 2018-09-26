//
// 2018-04-26, jjuiddong
// GIS 관련된 정의 모음
//
// vworld 3D coordinate system
//
//  /\ Z 
//  |
//  | 
//  |
//  ---------> X 
//  Ground = X-Z Plane
//
//
#pragma once


#include "../../../Common/Common/common.h"
using namespace common;
#include "../../../Common/Graphic11/graphic11.h"

#include "CFW1StateSaver.h"


namespace gis
{
	// using fo x=longitude, y=latitude
	struct Vector2d
	{
		double x, y;

		Vector2d() : x(0), y(0) {}
		Vector2d(const double x0, const double y0) :x(x0), y(y0) {}
		
		common::Vector2 ToVector2() const { 
			return common::Vector2((float)x, (float)y);
		}
		bool operator==(const Vector2d &rhs) {
			return (x == rhs.x) && (y == rhs.y);
		}
		bool operator!=(const Vector2d &rhs) {
			return !operator==(rhs);
		}
	};

	struct Vector3d
	{
		double x, y, z;

		Vector3d() : x(0), y(0), z(0) {}
		Vector3d(const double x0, const double y0, const double z0) :x(x0), y(y0), z(z0) {}
	};

	// aabbox3dd
	struct sAABBox
	{
		Vector3d _min;
		Vector3d _max;
	};

	struct sTileLoc
	{
		int level;
		int xLoc;
		int yLoc;
	};


	// 4 point bounding rect
	//  Y axis (lonLat = 0,90)
	//  /\       
	//   |       / Z axis  (lonLat = 180,0)
	//   |     /
	//   |   /
	//   | /
	//  --------------> X axis (lonLat = 90, 0)
	//
	struct sBoundingRect
	{
		Vector3 lt; // left top (node tile loc.x, y)
		Vector3 rt; // right top (node tile loc.x+1, y
		Vector3 rb; // right bottom (node tile loc.x+1, y+1)
		Vector3 lb; // left bottom (node tile loc.x, y+1)
		Vector3 center;
		Plane plane;
	};


	// vworld tile
	//
	//  /\ tileLoc Y (row)
	//  |
	//  | 
	//  |
	//  ---------> tileLoc X (col)
	// 
	struct sTile
	{
		// 2 ---- 3
		// |      |
		// |      |
		// 0 ---- 1
		Vector2d lonLat[4];
		sTileLoc loc;
	};

	// Calc WGS84 Coordinate System
	Vector2d TileLoc2WGS84(const int level, const int xLoc, const int yLoc);
	Vector2d TileLoc2WGS84(const sTileLoc &loc);
	sTileLoc WGS842TileLoc(const Vector2d &lonLat);

	// Transform X-Z 2D Coordinate System 
	Vector2d Pos2WGS84(const Vector3 &pos
		, const Vector2d &leftBottomLonLat, const Vector2d &rightTopLonLat
		, const sRectf &rect);
	Vector2d Pos2WGS84(const Vector3 &globalPos);
	Vector3 WGS842Pos(const Vector2d &lonLat);
	Vector3 GetRelationPos(const Vector3 &globalPos, const float scale = 1.f);
	Vector3 GetGlobalPos(const Vector3 &relPos, const float scale = 1.f);
	
	// Trasnform X-Y-Z 3D Sphere Coordinate System
	Vector3 GetNormal(const Vector2d &lonLat);
	Vector3 WGS842PosSphere(const Vector2d &lonLat);


	// meter -> 3D unit
	float Meter23DUnit(const float meter);
}


namespace
{
	const static char *g_textureDir = "..\\media\\VWorld";
	const static char *g_heightmapDir = "..\\media\\VWorld";
	const static char *g_mediaDir = "..\\media\\VWorld";

	const static float g_globeRadius = 100000.f;

	// Relation Space System
	//static const int g_offsetLv = 7;
	//static const int g_offsetXLoc = 1088;
	//static const int g_offsetYLoc = 442;
	
	// Global Space System
	static const int g_offsetLv = 0;
	static const int g_offsetXLoc = 0;
	static const int g_offsetYLoc = 0;
}


#include "UTM.h"
#include "vworldwebdownloader.h"
#include "quadtree.h"
#include "heightmap.h"
#include "tiletexture.h"
#include "poireader.h"
#include "real3dmodelindexreader.h"
#include "xdoreader.h"
#include "quadtile.h"
#include "quadtilemanager.h"
#include "terrainquadtree.h"
#include "route.h"
#include "root.h"
