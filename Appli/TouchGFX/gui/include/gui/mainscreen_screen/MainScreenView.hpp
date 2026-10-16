#ifndef MAINSCREENVIEW_HPP
#define MAINSCREENVIEW_HPP

#include <gui_generated/mainscreen_screen/MainScreenViewBase.hpp>
#include <gui/mainscreen_screen/MainScreenPresenter.hpp>
#include <touchgfx/Callback.hpp>

class MainScreenView : public MainScreenViewBase
{
public:
    MainScreenView();
    virtual ~MainScreenView() {}
    virtual void setupScreen();
    virtual void tearDownScreen();
protected:
    touchgfx::Callback<MainScreenView, const touchgfx::Image&, const touchgfx::ClickEvent&> image1ClickCallback;

    void image1Clicked(const touchgfx::Image& source, const touchgfx::ClickEvent& event);
};

#endif // MAINSCREENVIEW_HPP
