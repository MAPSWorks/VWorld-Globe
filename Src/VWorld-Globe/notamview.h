//
// 2018-06-12, jjuiddong
// Route View
//
#pragma once


class cNotamView : public framework::cDockWindow
{
public:
	cNotamView(const StrId &name);
	virtual ~cNotamView();

	virtual void OnUpdate(const float deltaSeconds) override;
	virtual void OnRender(const float deltaSeconds) override;


protected:
	bool ReadTrackPos();
	bool ParseNotam(const char *str);


public:
	cRoute m_route;
	Str512 m_notamStr;
};
