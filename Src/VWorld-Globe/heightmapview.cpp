
#include "stdafx.h"
#include "heightmapview.h"
#include "vworldglobe.h"
#include "mapview.h"

using namespace graphic;
using namespace framework;


cHeightmapView::cHeightmapView(const StrId &name)
	: framework::cDockWindow(name)
	, m_heightTex(NULL)
{
}

cHeightmapView::~cHeightmapView()
{
}


bool cHeightmapView::Init(cRenderer &renderer)
{
	const Vector3 eyePos(-10, 10, -10);
	const Vector3 lookAt(0, 0, 0);
	m_camera.SetCamera(eyePos, lookAt, Vector3(0, 1, 0));
	m_camera.SetProjection(MATH_PI / 4.f, m_rect.Width() / m_rect.Height(), 1, 1000000.f);
	m_camera.SetViewPort(m_rect.Width(), m_rect.Height());

	sf::Vector2u size((u_int)m_rect.Width() - 15, (u_int)m_rect.Height() - 50);
	cViewport vp = renderer.m_viewPort;
	vp.m_vp.Width = (float)size.x;
	vp.m_vp.Height = (float)size.y;
	m_renderTarget.Create(renderer, vp, DXGI_FORMAT_R8G8B8A8_UNORM, true, true
		, DXGI_FORMAT_D24_UNORM_S8_UINT);


	StrPath fileName;
	fileName.Format("../media/WorldTerrain/SRTM2/8/1813/1813_4364.bil");
	if (fileName.IsFileExist())
	{
		const int fileSize = (int)common::FileSize(fileName.c_str());
		RETV(fileSize <= 0, false);
		const int vtxCnt = (int)sqrt(fileSize / 2);
		RETV(vtxCnt <= 0, false);
		const int bytesSize = sizeof(WORD);

		std::ifstream ifs(fileName.c_str(), std::ios::binary);
		if (!ifs.is_open())
			return false;

		BYTE *pdata = new BYTE[vtxCnt * vtxCnt * bytesSize];
		ZeroMemory(pdata, vtxCnt * vtxCnt * bytesSize);
		ifs.read((char*)pdata, vtxCnt * vtxCnt * bytesSize);

		float *map = new float[vtxCnt * vtxCnt];
		ZeroMemory(map, vtxCnt * vtxCnt * sizeof(float));

		for (int i = 0; i < vtxCnt * vtxCnt; i++)
			map[i] = (float)(*(short*)&pdata[i*bytesSize]) * 0.0001f;

		cTexture *tex = new cTexture();
		const bool result = tex->Create(renderer, vtxCnt, vtxCnt
			, DXGI_FORMAT_R32_FLOAT
			, map, vtxCnt * sizeof(float));

		m_heightTex = tex;
		tex->WritePNGFile(renderer, "heightmap.png");

		delete[] pdata;
		delete[] map;
	}

	m_quad.Create(renderer, 5, 5, Vector3(0, 0, 0)
		, eVertexType::POSITION | eVertexType::NORMAL | eVertexType::TEXTURE0
		, "heightmap.png");

	return true;
}


void cHeightmapView::OnUpdate(const float deltaSeconds)
{
}


void cHeightmapView::OnPreRender(const float deltaSeconds)
{
	cRenderer &renderer = GetRenderer();
	cAutoCam cam(&m_camera);

	renderer.UnbindTextureAll();

	GetMainCamera().Bind(renderer);
	GetMainLight().Bind(renderer);

	if (m_renderTarget.Begin(renderer))
	{
		CommonStates states(renderer.GetDevice());
		renderer.GetDevContext()->RSSetState(states.CullCounterClockwise());
		m_quad.Render(renderer);
	}
	m_renderTarget.End(renderer);
}


void cHeightmapView::OnRender(const float deltaSeconds)
{
	ImVec2 pos = ImGui::GetCursorScreenPos();
	m_viewPos = { (int)(pos.x), (int)(pos.y) };
	m_viewRect = { pos.x + 5, pos.y, pos.x + m_rect.Width() - 30, pos.y + m_rect.Height() - 50 };
	ImGui::Image(m_renderTarget.m_resolvedSRV, ImVec2(m_rect.Width() - 15, m_rect.Height() - 50));

	// HUD
	const float windowAlpha = 0.0f;
	bool isOpen = true;
	ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse;
	ImGui::SetNextWindowPos(pos);
	ImGui::SetNextWindowSize(ImVec2(min(m_viewRect.Width(), 500), 120));
	if (ImGui::Begin("Information", &isOpen, ImVec2(500.f, 120.f), windowAlpha, flags))
	{
		ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);

		static int locs[3];
		ImGui::InputInt3("level-xLoc-yLoc", locs);
		if (ImGui::Button("dump heightmap"))
		{
			cRenderer &renderer = GetRenderer();
			cViewer *viewer = (cViewer*)g_application;
			cTerrainQuadTree &terrain = viewer->m_mapView->m_quadTree;
			if (cQuadTile *tile = terrain.m_tileMgr.GetTile(renderer, locs[0], locs[1], locs[2], {}))
			{
				if (tile->m_hmap)
				{
					using namespace std;
					StrPath path;
					path.Format("tile_dump_%d_%04d_%04d.txt", locs[0], locs[1], locs[2]);
					ofstream ofs(path.c_str());
					if (ofs.is_open())
					{
						cHeightmap &hmap = *tile->m_hmap;
						for (int y = 0; y < hmap.m_height; ++y)
						{
							for (int x = 0; x < hmap.m_width; ++x)
							{
								ofs << hmap.m_data[y*hmap.m_width + x] << "\t";
							}
							ofs << endl;
						}
					}
				}
			}
		}

		ImGui::End();
	}
}


void cHeightmapView::OnResizeEnd(const framework::eDockResize::Enum type, const sRectf &rect)
{
	if (type == eDockResize::DOCK_WINDOW)
	{
		m_owner->RequestResetDeviceNextFrame();
	}
}


void cHeightmapView::UpdateLookAt()
{
	GetMainCamera().MoveCancel();

	const Plane ground(Vector3(0, 1, 0), 0);
	const float centerX = GetMainCamera().m_width / 2;
	const float centerY = GetMainCamera().m_height / 2;
	const Ray ray = GetMainCamera().GetRay((int)centerX, (int)centerY);
	const float distance = ground.Collision(ray.dir);
	if (distance < -0.2f)
	{
		GetMainCamera().m_lookAt = ground.Pick(ray.orig, ray.dir);
	}
	else
	{ // horizontal viewing
		const Vector3 lookAt = GetMainCamera().m_eyePos + GetMainCamera().GetDirection() * 50.f;
		GetMainCamera().m_lookAt = lookAt;
	}

	GetMainCamera().UpdateViewMatrix();
}


// 휠을 움직였을 때,
// 카메라 앞에 박스가 있다면, 박스 정면에서 멈춘다.
void cHeightmapView::OnWheelMove(const float delta, const POINT mousePt)
{
	UpdateLookAt();

	float len = 0;
	const Plane ground(Vector3(0, 1, 0), 0);
	const Ray ray = GetMainCamera().GetRay(mousePt.x, mousePt.y);
	Vector3 lookAt = ground.Pick(ray.orig, ray.dir);
	len = (ray.orig - lookAt).Length();

	// zoom in/out
	float zoomLen = 0;
	if (len > 100)
		zoomLen = len * 0.1f;
	else if (len > 50)
		zoomLen = max(1.f, (len / 2.f));
	else
		zoomLen = (len / 3.f);

	GetMainCamera().Zoom(ray.dir, (delta < 0) ? -zoomLen : zoomLen);
}


// Handling Mouse Move Event
void cHeightmapView::OnMouseMove(const POINT mousePt)
{
	const POINT delta = { mousePt.x - m_mousePos.x, mousePt.y - m_mousePos.y };
	m_mousePos = mousePt;

	if (m_mouseDown[0])
	{
		Vector3 dir = GetMainCamera().GetDirection();
		Vector3 right = GetMainCamera().GetRight();
		dir.y = 0;
		dir.Normalize();
		right.y = 0;
		right.Normalize();

		GetMainCamera().MoveRight(-delta.x * m_rotateLen * 0.001f);
		GetMainCamera().MoveFrontHorizontal(delta.y * m_rotateLen * 0.001f);
	}
	else if (m_mouseDown[1])
	{
		m_camera.Yaw2(delta.x * 0.005f, Vector3(0, 1, 0));
		m_camera.Pitch2(delta.y * 0.005f, Vector3(0, 1, 0));
	}
	else if (m_mouseDown[2])
	{
		const float len = GetMainCamera().GetDistance();
		GetMainCamera().MoveRight(-delta.x * len * 0.001f);
		GetMainCamera().MoveUp(delta.y * len * 0.001f);
	}
}


// Handling Mouse Button Down Event
void cHeightmapView::OnMouseDown(const sf::Mouse::Button &button, const POINT mousePt)
{
	m_mousePos = mousePt;
	UpdateLookAt();
	SetCapture();
	const Plane ground(Vector3(0, 1, 0), 0);

	switch (button)
	{
	case sf::Mouse::Left:
	{
		m_mouseDown[0] = true;
		const Ray ray = GetMainCamera().GetRay(mousePt.x, mousePt.y);
		Vector3 p1 = ground.Pick(ray.orig, ray.dir);
		m_rotateLen = (p1 - ray.orig).Length();// min(500.f, (p1 - ray.orig).Length());
	}
	break;

	case sf::Mouse::Right:
	{
		m_mouseDown[1] = true;

		const Ray ray = GetMainCamera().GetRay(mousePt.x, mousePt.y);
		Vector3 target = ground.Pick(ray.orig, ray.dir);
		const float len = (GetMainCamera().GetEyePos() - target).Length();
	}
	break;

	case sf::Mouse::Middle:
		m_mouseDown[2] = true;
		break;
	}
}


void cHeightmapView::OnMouseUp(const sf::Mouse::Button &button, const POINT mousePt)
{
	const POINT delta = { mousePt.x - m_mousePos.x, mousePt.y - m_mousePos.y };
	m_mousePos = mousePt;
	ReleaseCapture();

	switch (button)
	{
	case sf::Mouse::Left:
		m_mouseDown[0] = false;
		break;
	case sf::Mouse::Right:
		m_mouseDown[1] = false;
		break;
	case sf::Mouse::Middle:
		m_mouseDown[2] = false;
		break;
	}
}


void cHeightmapView::OnEventProc(const sf::Event &evt)
{
	ImGuiIO& io = ImGui::GetIO();
	switch (evt.type)
	{
	case sf::Event::KeyPressed:
		switch (evt.key.code)
		{
		case sf::Keyboard::Return:
			break;

		case sf::Keyboard::Space:
			break;

		case sf::Keyboard::Left: m_camera.MoveRight(-0.5f); break;
		case sf::Keyboard::Right: m_camera.MoveRight(0.5f); break;
		case sf::Keyboard::Up: m_camera.MoveUp(0.5f); break;
		case sf::Keyboard::Down: m_camera.MoveUp(-0.5f); break;
		}
		break;

	case sf::Event::MouseMoved:
	{
		cAutoCam cam(&m_camera);

		POINT curPos;
		GetCursorPos(&curPos); // sf::event mouse position has noise so we use GetCursorPos() function
		ScreenToClient(m_owner->getSystemHandle(), &curPos);
		POINT pos = { curPos.x - m_viewPos.x, curPos.y - m_viewPos.y };
		OnMouseMove(pos);
	}
	break;

	case sf::Event::MouseButtonPressed:
	case sf::Event::MouseButtonReleased:
	{
		cAutoCam cam(&m_camera);

		POINT curPos;
		GetCursorPos(&curPos); // sf::event mouse position has noise so we use GetCursorPos() function
		ScreenToClient(m_owner->getSystemHandle(), &curPos);
		const POINT pos = { curPos.x - m_viewPos.x, curPos.y - m_viewPos.y };
		if (sf::Event::MouseButtonPressed == evt.type)
		{
			if (m_viewRect.IsIn((float)curPos.x, (float)curPos.y))
				OnMouseDown(evt.mouseButton.button, pos);
		}
		else
		{
			OnMouseUp(evt.mouseButton.button, pos);
		}
	}
	break;

	case sf::Event::MouseWheelScrolled:
	{
		cAutoCam cam(&m_camera);

		POINT curPos;
		GetCursorPos(&curPos); // sf::event mouse position has noise so we use GetCursorPos() function
		ScreenToClient(m_owner->getSystemHandle(), &curPos);
		const POINT pos = { curPos.x - m_viewPos.x, curPos.y - m_viewPos.y };
		OnWheelMove(evt.mouseWheelScroll.delta, pos);
	}
	break;
	}
}


void cHeightmapView::OnResetDevice()
{
	cRenderer &renderer = GetRenderer();

	// update viewport
	sRectf viewRect = { 0, 0, m_rect.Width() - 15, m_rect.Height() - 50 };
	m_camera.SetViewPort(viewRect.Width(), viewRect.Height());

	cViewport vp = GetRenderer().m_viewPort;
	vp.m_vp.Width = viewRect.Width();
	vp.m_vp.Height = viewRect.Height();
	m_renderTarget.Create(renderer, vp, DXGI_FORMAT_R8G8B8A8_UNORM, true, true, DXGI_FORMAT_D24_UNORM_S8_UINT);
}

