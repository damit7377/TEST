#include <gui/mainscreen_screen/MainScreenView.hpp>

MainScreenView::MainScreenView() :
    image1ClickCallback(this, &MainScreenView::image1Clicked)
{
    image1.setClickAction(image1ClickCallback);
}

void MainScreenView::setupScreen()
{
    MainScreenViewBase::setupScreen();
}

void MainScreenView::tearDownScreen()
{
    MainScreenViewBase::tearDownScreen();
}

void MainScreenView::image1Clicked(const touchgfx::Image& source, const touchgfx::ClickEvent& event)
{
    (void)source;
    if (event.getType() == touchgfx::ClickEvent::RELEASED)
    {
        application().gotoscreenScreenNoTransition();
    }
}
