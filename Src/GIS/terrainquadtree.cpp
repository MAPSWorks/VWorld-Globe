
#include "stdafx.h"
#include "terrainquadtree.h"

using namespace graphic;

cQuadTileManager cTerrainQuadTree::m_tileMgr;

// sQuadData
sQuadData::sQuadData() 
{
	ZeroMemory(level, sizeof(level));
	tile = NULL;
}
//


// Quad Tree Traverse Stack Memory (no multithread safe!!)
struct sData
{
	sRectf rect;
	gis::sBoundingRect brect;
	int level;
	sQuadTreeNode<sQuadData> *node;
};
sData g_stack[1024];
//

cTerrainQuadTree::cTerrainQuadTree()
	: m_isShowQuadTree(false)
	, m_isShowTexture(true)
	, m_isShowFacility(true)
	, m_isLight(true)
	, m_isCalcResDistribution(false)
	, m_showQuadCount(0)
	, m_isShowLevel(false)
	, m_isShowLoc(false)
	, m_isShowPoi1(true)
	, m_isShowPoi2(false)
	, m_isShowDistribute(true)
	, m_techniqType(0)
	, m_distributeType(0)
{
	m_techName[0] = "Light";
	m_techName[1] = "NoTexture";
	m_techName[2] = "Heightmap";
	m_techName[3] = "Light_Heightmap";
	m_techName[4] = "Unlit";
	m_txtColor = Vector3(1, 0, 0);
}

cTerrainQuadTree::cTerrainQuadTree(sRectf &rect)
{
}

cTerrainQuadTree::~cTerrainQuadTree()
{
	Clear();
}


bool cTerrainQuadTree::Create(graphic::cRenderer &renderer
	, const bool isResDistriubution)
{
	sVertexNormTex vtx;
	vtx.p = Vector3(0, 0, 0);
	vtx.n = Vector3(0, 1, 0);
	vtx.u = 0;
	vtx.v = 0;
	m_vtxBuff.Create(renderer, 1, sizeof(sVertexNormTex), &vtx);

	if (!m_shader.Create(renderer, "../media/shader11/tess-pos-norm-tex.fxo", "Unlit"
		, eVertexType::POSITION | eVertexType::NORMAL | eVertexType::TEXTURE0))
	{
		return false;
	}

	m_mtrl.InitWhite();
	m_mtrl.m_specular = Vector4(0, 0, 0, 1);

	// root node 10 x 5 tiles
	sRectf rootRect = sRectf::Rect(0, 0, 1 << cQuadTree<sQuadData>::MAX_LEVEL, 1 << cQuadTree<sQuadData>::MAX_LEVEL);
	m_qtree.m_rootRect = sRectf::Rect(0, 0, rootRect.right*10, rootRect.bottom*5);

	m_text.Create(renderer, 18.f, true, "Consolas");

	ReadResourceTDistribution("../Media/WorldTerrain/ResFiles/res_texture_distribution.txt");
	ReadResourceHDistribution("../Media/WorldTerrain/ResFiles/res_height_distribution.txt");

	return true;
}


// lonLat : ī�޶� ����Ű�� ���浵
void cTerrainQuadTree::Update(graphic::cRenderer &renderer, 
	const gis::Vector2d &camLonLat, const float deltaSeconds)
{
	m_tileMgr.Update(renderer, *this, camLonLat, deltaSeconds);
}


void cTerrainQuadTree::Render(graphic::cRenderer &renderer
	, const float deltaSeconds
	, const graphic::cFrustum &frustum
	, const Ray &ray 
	, const Ray &mouseRay
)
{
	BuildQuadTree(frustum, ray);

	if (m_isShowTexture)
	{
		RenderTessellation(renderer, deltaSeconds, frustum);
	}

	if (m_isShowFacility)
		RenderFacility(renderer, deltaSeconds, frustum);

	if (m_isShowQuadTree)
		RenderQuad(renderer, deltaSeconds, frustum, mouseRay);

	if (m_isShowDistribute)
		RenderResDistribution(renderer, deltaSeconds, frustum, m_distributeType);
}


void cTerrainQuadTree::BuildQuadTree(const graphic::cFrustum &frustum
	, const Ray &ray)
{
	m_qtree.Clear();

	for (int x = 0; x < 10; ++x)
	{
		for (int y = 0; y < 5; ++y)
		{
			sQuadTreeNode<sQuadData> *node = new sQuadTreeNode<sQuadData>;
			node->xLoc = x;
			node->yLoc = y;
			node->level = 0;
			m_qtree.Insert(node);
		}
	}

	int sp = 0;
	for (auto &node : m_qtree.m_roots)
	{
		//sRectf rect = m_qtree.GetNodeRect(node);
		//gis::sBoundingRect brect = m_qtree.GetNodeBoundingRect(node);
		g_stack[sp++] = { {}, {}, node->level, node };
	}

	while (sp > 0)
	{
		//const sRectf rect = g_stack[sp - 1].rect;
		//const gis::sBoundingRect brect = g_stack[sp - 1].brect;
		const int lv = g_stack[sp - 1].level;
		sQuadTreeNode<sQuadData> *parentNode = g_stack[sp - 1].node;
		const sRectf rect = m_qtree.GetNodeRect(parentNode);
		const gis::sBoundingRect brect = m_qtree.GetNodeBoundingRect(parentNode);

		--sp;

		const float maxH = GetMaximumHeight(parentNode);
		//if (!IsContain(frustum, rect, maxH))
		//	continue;
		if (!IsContain(frustum, brect, maxH))
			continue;

		const float hw = rect.Width() / 2.f;
		const float hh = rect.Height() / 2.f;
		const bool isMinSize = hw < 1.f;
		if (isMinSize)
			continue;

		bool isSkipThisNode = false;
		{
			//const Vector3 center = Vector3(rect.Center().x, maxH, rect.Center().y);
			//const float distance = center.Distance(ray.orig);
			const float distance = brect.center.Distance(ray.orig);
			const int curLevel = GetLevel(distance);
			isSkipThisNode = (lv >= curLevel);
		}

		Vector3 childPoint[4]; // Rect�� ������ 4��
		childPoint[0] = Vector3(rect.left, 0, rect.top);
		childPoint[1] = Vector3(rect.right, 0, rect.top);
		childPoint[2] = Vector3(rect.left, 0, rect.bottom);
		childPoint[3] = Vector3(rect.right, 0, rect.bottom);

		bool isChildShow = false;
		if (isSkipThisNode && (hw > 2.f)) // test child node distance
		{
			for (int i = 0; i < 4; ++i)
			{
				const float distance = childPoint[i].Distance(ray.orig);
				const int curLevel = GetLevel(distance);
				if (lv < curLevel)
				{
					isChildShow = true;
					break;
				}
			}
		}

		if (isSkipThisNode && !isChildShow)
			continue;

		m_qtree.InsertChildren(parentNode);

		g_stack[sp++] = { sRectf::Rect(rect.left, rect.top, hw, hh), {}, lv + 1, parentNode->children[0] };
		g_stack[sp++] = { sRectf::Rect(rect.left + hw, rect.top, hw, hh), {}, lv+1, parentNode->children[1] };
		g_stack[sp++] = { sRectf::Rect(rect.left, rect.top + hh, hw, hh), {}, lv+1, parentNode->children[2] };
		g_stack[sp++] = { sRectf::Rect(rect.left + hw, rect.top + hh, hw, hh), {}, lv+1, parentNode->children[3] };

		assert(sp < 1024);
	}
}


// ���� ���
void cTerrainQuadTree::RenderTessellation(graphic::cRenderer &renderer
	, const float deltaSeconds
	, const graphic::cFrustum &frustum )
{
	m_shader.SetTechnique(m_techName[m_techniqType]);
	m_shader.Begin();
	m_shader.BeginPass(renderer, 0);

	renderer.m_cbLight.Update(renderer, 1);
	renderer.m_cbMaterial = m_mtrl.GetMaterial();
	renderer.m_cbMaterial.Update(renderer, 2);

	int sp = 0;
	for (auto &node : m_qtree.m_roots)
	{
		//sRectf rect = m_qtree.GetNodeRect(node);
		//gis::sBoundingRect brect = m_qtree.GetNodeBoundingRect(node);
		g_stack[sp++] = { {}, {}, node->level, node };
	}

	while (sp > 0)
	{
		sQuadTreeNode<sQuadData> *node = g_stack[sp - 1].node;
		//const sRectf rect = g_stack[sp - 1].rect;
		//const gis::sBoundingRect brect = g_stack[sp - 1].brect;
		sRectf rect = m_qtree.GetNodeRect(node);
		gis::sBoundingRect brect = m_qtree.GetNodeBoundingRect(node);
		--sp;

		const float maxH = GetMaximumHeight(node);
		//if (!IsContain(frustum, rect, maxH))
		//	continue;
		if (!IsContain(frustum, brect, maxH))
			continue;

		// leaf node?
		if (!node->children[0])
		{
			cQuadTile *tile = m_tileMgr.GetTile(renderer, node->level, node->xLoc, node->yLoc, rect);
			tile->m_renderFlag = g_root.m_renderFlag;				
			node->data.tile = tile;
			m_tileMgr.LoadResource(renderer, *this, *tile, node->level, node->xLoc, node->yLoc, rect);
			//m_tileMgr.Smooth(renderer, *this, node);

			tile->Render(renderer, deltaSeconds, node->data.level);

			if (m_isShowPoi1 || m_isShowPoi2)
				tile->RenderPoi(renderer, deltaSeconds, m_isShowPoi1, m_isShowPoi2);
		}
		else
		{
			//const sRectf rect = m_qtree.GetNodeRect(node);
			cQuadTile *tile = m_tileMgr.GetTile(renderer, node->level, node->xLoc, node->yLoc, rect);
			tile->m_renderFlag = g_root.m_renderFlag;
			node->data.tile = tile;
			m_tileMgr.LoadResource(renderer, *this, *tile, node->level, node->xLoc, node->yLoc, rect);

			if (m_isShowPoi1 || m_isShowPoi2)
				tile->RenderPoi(renderer, deltaSeconds, m_isShowPoi1, m_isShowPoi2);

			for (int i = 0; i < 4; ++i)
			{
				if (sQuadTreeNode<sQuadData> *cnode = node->children[i])
				{
					g_stack[sp].node = cnode;
					//g_stack[sp].rect = m_qtree.GetNodeRect(cnode);
					//g_stack[sp].brect = m_qtree.GetNodeBoundingRect(cnode);
					sp++;
				}
			}
		}
	}

	renderer.UnbindShaderAll();
}


// �ǹ����
void cTerrainQuadTree::RenderFacility(graphic::cRenderer &renderer
	, const float deltaSeconds
	, const graphic::cFrustum &frustum)
{
	int sp = 0;
	for (auto &node : m_qtree.m_roots)
	{
		sRectf rect = m_qtree.GetNodeRect(node);
		g_stack[sp++] = { rect, {}, node->level, node };
	}

	while (sp > 0)
	{
		sQuadTreeNode<sQuadData> *node = g_stack[sp - 1].node;
		--sp;

		const float maxH = GetMaximumHeight(node);
		const sRectf rect = m_qtree.GetNodeRect(node);
		if (!IsContain(frustum, rect, maxH))
			continue;

		// leaf node?
		if (!node->children[0])
		{
			cQuadTile *tile = m_tileMgr.GetTile(renderer, node->level, node->xLoc, node->yLoc, rect);
			tile->RenderFacility(renderer, deltaSeconds);
		}
		else
		{
			const sRectf rect = m_qtree.GetNodeRect(node);
			cQuadTile *tile = m_tileMgr.GetTile(renderer, node->level, node->xLoc, node->yLoc, rect);
			for (int i = 0; i < 4; ++i)
				if (node->children[i])
					g_stack[sp++].node = node->children[i];
		}
	}

	renderer.UnbindShaderAll();
}


// QaudTree ������ ����Ѵ�.
// QuadRect, Level, xLoc, yLoc
void cTerrainQuadTree::RenderQuad(graphic::cRenderer &renderer
	, const float deltaSeconds
	, const graphic::cFrustum &frustum
	, const Ray &ray)
{
	cShader11 *shader = renderer.m_shaderMgr.FindShader(eVertexType::POSITION);
	assert(shader);
	shader->SetTechnique("Unlit");
	shader->Begin();
	shader->BeginPass(renderer, 0);

	struct sInfo
	{
		sRectf r;
		gis::sBoundingRect br;
		sQuadTreeNode<sQuadData> *node;
	};
	sInfo showRects[512];
	int showRectCnt = 0;
	vector<std::pair<sRectf, cColor>> ars;
	m_showQuadCount = 0;

	int sp = 0;
	for (auto &node : m_qtree.m_roots)
	{
		//sRectf rect = m_qtree.GetNodeRect(node);
		g_stack[sp++] = { {}, {}, node->level, node };
	}

	// Render Quad-Tree
	while (sp > 0)
	{
		sQuadTreeNode<sQuadData> *node = g_stack[sp - 1].node;
		sRectf rect = m_qtree.GetNodeRect(node);
		gis::sBoundingRect brect = m_qtree.GetNodeBoundingRect(node);
		--sp;

		const float maxH = GetMaximumHeight(node);
		//const sRectf rect = m_qtree.GetNodeRect(node);
		//const bool isShow = IsContain(frustum, rect, maxH);
		const bool isShow = IsContain(frustum, brect, maxH);
		if (!isShow)
			continue;

		// leaf node?
		if (!node->children[0])
		{
			cQuadTile *tile = m_tileMgr.GetTile(renderer, node->level, node->xLoc, node->yLoc, rect);
			if (tile)
			{
				//if (tile->m_facility && tile->m_facilityIndex)
				//{
				//	const Vector3 norm = tile->m_facilityIndex->m_objs[0].normal;
				//	const Vector3 p0 = tile->m_mod.m_transform.pos;
				//	const Vector3 p1 = tile->m_mod.m_transform.pos + norm * 1.f;
				//	renderer.m_dbgLine.SetLine(p0, p1, 0.01f);
				//	renderer.m_dbgLine.Render(renderer);
				//}
			}

			RenderRect3D(renderer, deltaSeconds, brect, cColor::BLACK);

			//if (isShow && (showRectCnt < ARRAYSIZE(showRects)))
			if (showRectCnt < ARRAYSIZE(showRects))
			{
				showRects[showRectCnt].r = rect;
				showRects[showRectCnt].br = brect;
				showRects[showRectCnt].node = node;
				showRectCnt++;
			}

			//if (isShow)
				++m_showQuadCount;
		}
		else
		{
			for (int i = 0; i < 4; ++i)
				if (node->children[i])
					g_stack[sp++].node = node->children[i];
		}
	}

	// Render Show QuadNode
	for (int i = 0; i < showRectCnt; ++i)
	{
		const sRectf &rect = showRects[i].r;
		const gis::sBoundingRect &brect = showRects[i].br;
		RenderRect3D(renderer, deltaSeconds, brect, cColor::WHITE);
	}

	// Render north, south, west, east quad node
	Plane ground(Vector3(0, 1, 0), 0);
	const Vector3 relPos = ground.Pick(ray.orig, ray.dir);
	const Vector3 globalPos = m_qtree.GetGlobalPos(relPos);
	if (sQuadTreeNode<sQuadData> *node = m_qtree.GetNode(sRectf::Rect(globalPos.x, globalPos.z, 0, 0)))
	{
		const sRectf rect = m_qtree.GetNodeRect(node);
		ars.push_back({ rect, cColor::WHITE });

		if (sQuadTreeNode<sQuadData> *north = m_qtree.GetNorthNeighbor(node))
		{
			const sRectf r = m_qtree.GetNodeRect(north);
			ars.push_back({ r, cColor::RED });
		}

		if (sQuadTreeNode<sQuadData> *east = m_qtree.GetEastNeighbor(node))
		{
			const sRectf r = m_qtree.GetNodeRect(east);
			ars.push_back({ r, cColor::GREEN });
		}

		if (sQuadTreeNode<sQuadData> *south = m_qtree.GetSouthNeighbor(node))
		{
			const sRectf r = m_qtree.GetNodeRect(south);
			ars.push_back({ r, cColor::BLUE });
		}

		if (sQuadTreeNode<sQuadData> *west = m_qtree.GetWestNeighbor(node))
		{
			const sRectf r = m_qtree.GetNodeRect(west);
			ars.push_back({ r, cColor::YELLOW });
		}
	}

	for (auto &data : ars)
	{
		const sRectf &r = data.first;

		CommonStates state(renderer.GetDevice());
		renderer.GetDevContext()->RSSetState(state.CullNone());
		renderer.GetDevContext()->OMSetDepthStencilState(state.DepthNone(), 0);
		RenderRect3D(renderer, deltaSeconds, r, data.second);
		renderer.GetDevContext()->OMSetDepthStencilState(state.DepthDefault(), 0);
		renderer.GetDevContext()->RSSetState(state.CullCounterClockwise());
	}

	// Render Show QuadNode
	if (m_isShowLevel || m_isShowLoc)
	{
		FW1FontWrapper::CFW1StateSaver stateSaver;
		stateSaver.saveCurrentState(renderer.GetDevContext());

		for (int i = 0; i < showRectCnt; ++i)
		{
			const sRectf &rect = showRects[i].r;
			const gis::sBoundingRect &brect = showRects[i].br;
			const Vector3 offset = Vector3(rect.Width()*0.1f, 0, -rect.Height()*0.1f);
			//const Vector2 pos = GetMainCamera().GetScreenPos(Vector3(rect.left, 0, rect.bottom) + offset);
			const Vector2 pos = GetMainCamera().GetScreenPos(brect.center);

			m_text.SetColor(cColor(m_txtColor));

			if (m_isShowLevel)
			{
				WStr32 strLv;
				strLv.Format(L"%d", showRects[i].node->level);
				m_text.SetText(strLv.c_str());
				m_text.Render(renderer, pos.x, pos.y, false, false);
			}

			if (m_isShowLoc)
			{
				WStr32 strLoc;
				strLoc.Format(L"xyLoc = {%d, %d}", showRects[i].node->xLoc, showRects[i].node->yLoc);
				m_text.SetText(strLoc.c_str());
				m_text.Render(renderer, pos.x, pos.y+20, false, false);

				if (showRects[i].node->data.tile &&
					showRects[i].node->data.tile->m_replaceHeightLv >= 0)
				{
					strLoc.Format(L"rep h = %d", showRects[i].node->data.tile->m_replaceHeightLv);
					m_text.SetText(strLoc.c_str());
					m_text.Render(renderer, pos.x, pos.y + 40, false, false);
				}
			}
		}

		stateSaver.restoreSavedState();
	}
}


// resType: 0=texture, 1=heightmap
void cTerrainQuadTree::RenderResDistribution(graphic::cRenderer &renderer
	, const float deltaSeconds
	, const graphic::cFrustum &frustum
	, const int resType)
{
	const Vector3 eyePos = GetMainCamera().GetEyePos();

	CommonStates state(renderer.GetDevice());
	renderer.GetDevContext()->OMSetDepthStencilState(state.DepthNone(), 0);

	renderer.m_dbgBox.m_color = (0==resType)? cColor::RED : cColor::BLUE;
	renderer.m_dbgBox.SetBox(Transform());

	int cnt = 0;
	XMMATRIX tms[256];
	vector<Vector3> &distrib = (0 == resType) ? m_resTDistribute : m_resHDistribute;
	for (u_int i=0; i < distrib.size(); ++i)
	{
		auto &vtx = distrib[i];
		if (!frustum.IsIn(vtx))
			continue;

		Transform tfm;
		tfm.pos = vtx;
		const float dist = vtx.Distance(eyePos);
		tfm.scale = Vector3(1, 1, 1) * max(0.1f, (dist * 0.003f));
		tms[cnt++] = tfm.GetMatrixXM();

		if (cnt >= 256)
		{
			renderer.m_dbgBox.RenderInstancing(renderer, cnt, tms);
			cnt = 0;
		}
	}

	if (cnt > 0)
		renderer.m_dbgBox.RenderInstancing(renderer, cnt, tms);

	renderer.GetDevContext()->OMSetDepthStencilState(state.DepthDefault(), 0);
}


void cTerrainQuadTree::RenderRect3D(graphic::cRenderer &renderer
	, const float deltaSeconds
	, const sRectf &rect
	, const cColor &color
)
{
	Vector3 lines[] = {
		Vector3(rect.left, 0, rect.top)
		, Vector3(rect.right, 0, rect.top)
		, Vector3(rect.right, 0, rect.bottom)
		, Vector3(rect.left, 0, rect.bottom)
		, Vector3(rect.left, 0, rect.top)
	};

	renderer.m_rect3D.SetRect(renderer, lines, 5);

	renderer.m_cbPerFrame.m_v->mWorld = XMIdentity;
	renderer.m_cbPerFrame.Update(renderer);
	const Vector4 c = color.GetColor();
	renderer.m_cbMaterial.m_v->diffuse = XMVectorSet(c.x, c.y, c.z, c.w);
	renderer.m_cbMaterial.Update(renderer, 2);
	renderer.m_rect3D.m_vtxBuff.Bind(renderer);
	renderer.GetDevContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP);
	renderer.GetDevContext()->DrawInstanced(renderer.m_rect3D.m_lineCount, 1, 0, 0);
}


void cTerrainQuadTree::RenderRect3D(graphic::cRenderer &renderer
	, const float deltaSeconds
	, const gis::sBoundingRect &brect
	, const cColor &color
)
{
	Vector3 lines[] = {
		brect.lt, brect.rt, brect.rb, brect.lb, brect.lt
	};

	renderer.m_rect3D.SetRect(renderer, lines, 5);

	renderer.m_cbPerFrame.m_v->mWorld = XMIdentity;
	renderer.m_cbPerFrame.Update(renderer);
	const Vector4 c = color.GetColor();
	renderer.m_cbMaterial.m_v->diffuse = XMVectorSet(c.x, c.y, c.z, c.w);
	renderer.m_cbMaterial.Update(renderer, 2);
	renderer.m_rect3D.m_vtxBuff.Bind(renderer);
	renderer.GetDevContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP);
	renderer.GetDevContext()->DrawInstanced(renderer.m_rect3D.m_lineCount, 1, 0, 0);
}


// node�� ���� ���� ���̸� �����Ѵ�.
// tile�� ������, �θ��带 �˻��ؼ� �����Ѵ�.
float cTerrainQuadTree::GetMaximumHeight(const sQuadTreeNode<sQuadData> *node)
{
	const float DEFAULT_H = 2.f;
	RETV(!node, DEFAULT_H);

	const cQuadTile *tile = m_tileMgr.FindTile(node->level, node->xLoc, node->yLoc);
	if (tile && tile->m_hmap)
		return max(DEFAULT_H, tile->m_hmap->m_maxHeight);

	RETV(node->level <= 1, DEFAULT_H);

	const int xLoc = node->xLoc >> 1;
	const int yLoc = node->yLoc >> 1;
	auto parent = m_qtree.GetNode(node->level-1, xLoc, yLoc);
	return GetMaximumHeight(parent);
}


// �ٸ� ������ ���尡 ������ ���� ��, �׼����̼��� �̿��ؼ�, ���ֺ��� ������ ���ؽ� ������ �����ش�.
// �´�� �ִ� ������ �ܰ� �׼����̼� ����� �����ϰ� �ؼ�, �������� ������ �ʰ� �Ѵ�.
// When other levels of quads are adjacent, use tessellation to match the number of vertices in the facing edge.
// Make the outer tessellation coefficients of the quads equal, so that no seams occur.
void cTerrainQuadTree::CalcSeamlessLevel()
{
	m_loopCount = 0;
	int sp = 0;
	for (auto &node : m_qtree.m_roots)
	{
		sRectf rect = m_qtree.GetNodeRect(node);
		g_stack[sp++] = { rect, {}, 15, node };
	}

	while (sp > 0)
	{
		sQuadTreeNode<sQuadData> *node = g_stack[sp - 1].node;
		--sp;
		++m_loopCount;

		// leaf node?
		if (!node->children[0])
		{
			if (sQuadTreeNode<sQuadData> *north = m_qtree.GetNorthNeighbor(node))
				CalcQuadEdgeLevel(node, north, eDirection::NORTH);
			else
				node->data.level[eDirection::NORTH] = node->level;

			if (sQuadTreeNode<sQuadData> *east = m_qtree.GetEastNeighbor(node))
				CalcQuadEdgeLevel(node, east, eDirection::EAST);
			else
				node->data.level[eDirection::EAST] = node->level;

			if (sQuadTreeNode<sQuadData> *south = m_qtree.GetSouthNeighbor(node))
				CalcQuadEdgeLevel(node, south, eDirection::SOUTH);
			else
				node->data.level[eDirection::SOUTH] = node->level;

			if (sQuadTreeNode<sQuadData> *west = m_qtree.GetWestNeighbor(node))
				CalcQuadEdgeLevel(node, west, eDirection::WEST);
			else
				node->data.level[eDirection::WEST] = node->level;
		}
		else
		{
			for (int i = 0; i < 4; ++i)
				if (node->children[i])
					g_stack[sp++].node = node->children[i];
		}
	}
}


// �ٸ� ����� �¹��� ��, ���� ������ ���带 �迭�� �����Ѵ�.
// �׼����̼ǿ���, �ڽ��� ������, ������ ������ ����, �׼����̼� ���͸� ����Ѵ�.
// Save high-level quads to the array when they are mingled with other quads.
// In tessellation, compute the tessellation factor by comparing its level with the level of the edge.
void cTerrainQuadTree::CalcQuadEdgeLevel(sQuadTreeNode<sQuadData> *from, sQuadTreeNode<sQuadData> *to
	, const eDirection::Enum dir)
{
	//eDirection::Enum oppDir = GetOpposite(dir);

	//if (from->data.level[dir] < from->level)
	//	from->data.level[dir] = from->level;

	//if (to->data.level[oppDir] < from->data.level[dir])
	//	to->data.level[oppDir] = from->data.level[dir];

	//if (from->data.level[dir] < to->data.level[oppDir])
	//	from->data.level[dir] = to->data.level[oppDir];

	//from->data.level[dir] = to->level;
	
	if (from->data.level[dir] < to->level)
		from->data.level[dir] = to->level;
}


eDirection::Enum cTerrainQuadTree::GetOpposite(const eDirection::Enum type)
{
	switch (type)
	{
	case eDirection::NORTH: return eDirection::SOUTH;
	case eDirection::EAST: return eDirection::WEST;
	case eDirection::SOUTH: return eDirection::NORTH;
	case eDirection::WEST: return eDirection::EAST;
	default:
		assert(0);
		return eDirection::SOUTH;
	}
}


// frustum �ȿ� rect�� ���ԵǸ� true�� �����Ѵ�.
inline bool cTerrainQuadTree::IsContain(const graphic::cFrustum &frustum, const sRectf &rect
	, const float maxH)
{
	const float hw = rect.Width() / 2.f;
	const float hh = rect.Height() / 2.f;
	const Vector3 center = Vector3(rect.Center().x, 0, rect.Center().y);
	cBoundingBox bbox;
	//bbox.SetBoundingBox(center + Vector3(0,hh,0), Vector3(hw, hh, hh), Quaternion());
	//bbox.SetBoundingBox(center + Vector3(0, 0, 0), Vector3(hw, hh*4, hh), Quaternion());
	bbox.SetBoundingBox(center + Vector3(0, maxH/2.f, 0)
		, Vector3(hw, maxH/2.f, hh)
		, Quaternion());
	const bool reval = frustum.FrustumAABBIntersect(bbox);
	return reval;
}


// frustum �ȿ� bounding rect�� ���ԵǸ� true�� �����Ѵ�.
inline bool cTerrainQuadTree::IsContain(const graphic::cFrustum &frustum
	, const gis::sBoundingRect &brect, const float maxH)
{
	const float dot = brect.plane.N.DotProduct(*(Vector3*)&frustum.m_dir);
	return dot < 0;
}


// ī�޶�� �Ÿ��� ����, ���� ������ ����Ѵ�.
inline int cTerrainQuadTree::GetLevel(const float distance)
{
	int dist = (int)distance - 20;

#define CALC(lv) if ((dist >>= 1) < 1) return lv;

	CALC(cQuadTree<sQuadData>::MAX_LEVEL);
	CALC(cQuadTree<sQuadData>::MAX_LEVEL - 1);
	CALC(cQuadTree<sQuadData>::MAX_LEVEL - 2);
	CALC(cQuadTree<sQuadData>::MAX_LEVEL - 3);
	CALC(cQuadTree<sQuadData>::MAX_LEVEL - 4);
	CALC(cQuadTree<sQuadData>::MAX_LEVEL - 5);
	CALC(cQuadTree<sQuadData>::MAX_LEVEL - 6);
	CALC(cQuadTree<sQuadData>::MAX_LEVEL - 7);
	CALC(cQuadTree<sQuadData>::MAX_LEVEL - 8);
	CALC(cQuadTree<sQuadData>::MAX_LEVEL - 9);
	CALC(cQuadTree<sQuadData>::MAX_LEVEL - 10);
	CALC(cQuadTree<sQuadData>::MAX_LEVEL - 11);
	CALC(cQuadTree<sQuadData>::MAX_LEVEL - 12);
	CALC(cQuadTree<sQuadData>::MAX_LEVEL - 13);
	CALC(cQuadTree<sQuadData>::MAX_LEVEL - 14);
	CALC(cQuadTree<sQuadData>::MAX_LEVEL - 15);
	return 0;
}


// �ؽ��� ���ҽ� ���� ������ �д´�.
bool cTerrainQuadTree::ReadResourceTDistribution(const char *fileName)
{
	m_resTDistribute.clear();

	using namespace std;
	ifstream ifs(fileName);
	if (!ifs.is_open())
		return false;

	string line;
	while (getline(ifs, line))
	{
		stringstream ss(line);
		Vector3 pos;
		ss >> pos.x >> pos.z;
		m_resTDistribute.push_back(pos);
	}

	return true;
}


// ���̸� ���ҽ� ���� ������ �д´�.
bool cTerrainQuadTree::ReadResourceHDistribution(const char *fileName)
{
	m_resHDistribute.clear();

	using namespace std;
	ifstream ifs(fileName);
	if (!ifs.is_open())
		return false;

	string line;
	while (getline(ifs, line))
	{
		stringstream ss(line);
		Vector3 pos;
		ss >> pos.x >> pos.z;
		m_resHDistribute.push_back(pos);
	}

	return true;
}


// ���콺 ��ġ�� �������� ����� �����Ѵ�. (���� ���� ����)
// ����Ʈ���� Ž���ϸ� ����Ѵ�.
// return : long,lat ������
gis::Vector2d cTerrainQuadTree::GetLongLat(const Ray &ray)
{
	const Plane ground(Vector3(0, 1, 0), 0);
	const Vector3 relPos = ground.Pick(ray.orig, ray.dir);
	return GetWGS84(relPos);
}


// �������� �ش��ϴ� 3D ��ġ�� �����Ѵ�. (�����ǥ��)
Vector3 cTerrainQuadTree::Get3DPos(const gis::Vector2d &lonLat)
{
	const Vector3 globalPos = gis::WGS842Pos(lonLat);
	Vector3 relPos = gis::GetRelationPos(globalPos);
	const sQuadTreeNode<sQuadData> *node = m_qtree.GetNode(sRectf::Rect(globalPos.x, globalPos.z, 0,0));
	if (node && node->data.tile)
	{
		cQuadTile *tile = node->data.tile;

		const float u = (relPos.x - tile->m_rect.left) / tile->m_rect.Width();
		const float v = 1.f + (tile->m_rect.top - relPos.z) / tile->m_rect.Height();
		//relPos.y = (tile->m_hmap->GetHeight(Vector2(u,v)) - 0.1f) * 2500.f;
		relPos.y = tile->GetHeight(Vector2(u, v)) * 2500.f;
	}

	return relPos;
}


// 3D ��ǥ�� �ش��ϴ� �������� �����Ѵ�.
// 3D ��ǥ�� ���̰��� ���õȴ�.
// return: long-lat (������)
gis::Vector2d cTerrainQuadTree::GetWGS84(const Vector3 &relPos)
{
	const Vector3 globalPos = m_qtree.GetGlobalPos(relPos);
	const sQuadTreeNode<sQuadData> *node = m_qtree.GetNode(sRectf::Rect(globalPos.x, globalPos.z, 0, 0));
	if (!node)
		return {0,0};

	const sRectf rect = m_qtree.GetNodeRect(node);
	const gis::Vector2d lonLat1 = gis::TileLoc2WGS84(node->level, node->xLoc, node->yLoc);
	const gis::Vector2d lonLat2 = gis::TileLoc2WGS84(node->level, node->xLoc + 1, node->yLoc + 1);
	gis::Vector2d lonLat = gis::Pos2WGS84(relPos, lonLat1, lonLat2, rect);
	return lonLat;
}


// pos ��ġ�� ���� ���̸� �����Ѵ�.
// pos.xy = xz ��� ��ǥ
float cTerrainQuadTree::GetHeight(const Vector2 &relPos)
{
	//const Vector3 relPos = Vector3(pos.x, 0, pos.y);
	const Vector3 globalPos = m_qtree.GetGlobalPos(Vector3(relPos.x, 0, relPos.y));
	const sQuadTreeNode<sQuadData> *node = m_qtree.GetNode(sRectf::Rect(globalPos.x, globalPos.z, 0, 0));
	if (!node)
		return 0.f;

	const sQuadTreeNode<sQuadData> *p = node;
	const cQuadTile *tile = NULL;
	while (p && !tile)
	{
		tile = p->data.tile;
		p = p->parent;
	}
	if (!tile)
		return 0.f;

	const float u = (tile->m_rect.left - relPos.x) / tile->m_rect.Width();
	const float v = (tile->m_rect.top - relPos.y) / tile->m_rect.Height();
	return tile->GetHeight(Vector2(u, v)) * 2500.f;
}


std::pair<bool, Vector3> cTerrainQuadTree::Pick(const Ray &ray)
{
	sQuadTreeNode<sQuadData> *pnode = NULL; // parent node
	for (auto &node : m_qtree.m_roots)
	{
		if (!node->data.tile)
			continue;

		auto result = node->data.tile->Pick(ray);
		if (result.first)
		{
			pnode = node;
			break;
		}
	}

	RETV(!pnode, std::make_pair(false, Vector3(0,0,0)));

	const Ray newRay(ray.orig - ray.dir*1000.f, ray.dir);

	sQuadTreeNode<sQuadData> *pickNode = NULL;
	for (int i=0; i < 4; ++i)
	{
		// is Leaf?
		if (!pnode->children[0])
		{
			pickNode = pnode;
			break;
		}

		auto &node = pnode->children[i];
		if (!node->data.tile)
			continue;

		auto result = node->data.tile->Pick(newRay);
		if (!result.first)
			continue;

		pnode = node;
		i = -1; // because ++i
	}

	if (!pickNode)
	{
		// ���ػ� ��ŷ�� �����ϸ�, �θ��� ��ŷ��ǥ�� �����Ѵ�.
		return pnode->data.tile->Pick(newRay);
	}

	// detail picking pickNode
	return pickNode->data.tile->Pick(newRay);
}


void cTerrainQuadTree::Clear()
{
	m_tileMgr.Clear();
}
