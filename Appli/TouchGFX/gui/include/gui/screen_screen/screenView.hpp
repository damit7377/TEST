#ifndef SCREENVIEW_HPP
#define SCREENVIEW_HPP

#include <gui_generated/screen_screen/screenViewBase.hpp>
#include <gui/screen_screen/screenPresenter.hpp>
#include <touchgfx/Callback.hpp>
#include "ethernet_test.h"

class screenView : public screenViewBase
{
public:
    screenView();
    virtual ~screenView() {}
    virtual void setupScreen();
    virtual void tearDownScreen();
    virtual void handleTickEvent();
protected:
    touchgfx::Callback<screenView, const touchgfx::ButtonWithLabel&, const touchgfx::ClickEvent&> ethTestButtonCallback;

    void ethTestButtonClicked(const touchgfx::ButtonWithLabel& source,
                              const touchgfx::ClickEvent& event);

    ETH_TestResultTypeDef displayedEthTestResult;
    void updateEthTestResult(ETH_TestResultTypeDef result);
};

#endif // SCREENVIEW_HPP
