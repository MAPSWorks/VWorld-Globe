//
// 2018-06-12, jjuiddong
// Route View
//
#pragma once


class cRouteView : public framework::cDockWindow
{
public:
	cRouteView(const StrId &name);
	virtual ~cRouteView();

	virtual void OnUpdate(const float deltaSeconds) override;
	virtual void OnRender(const float deltaSeconds) override;


protected:
	bool ReadTrackPos();


public:
	cRoute m_route;
};
