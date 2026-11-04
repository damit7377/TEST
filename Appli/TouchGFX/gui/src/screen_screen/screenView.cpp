#include <gui/screen_screen/screenView.hpp>

screenView::screenView() :
    ethTestButtonCallback(this, &screenView::ethTestButtonClicked),
    displayedEthTestResult(ETH_TEST_NOT_READY)
{
    buttonWithLabel_ETHTEST.setClickAction(ethTestButtonCallback);
}

void screenView::setupScreen()
{
    screenViewBase::setupScreen();
}

void screenView::tearDownScreen()
{
    screenViewBase::tearDownScreen();
}

void screenView::handleTickEvent()
{
    const ETH_TestResultTypeDef result = ETH_Loopback_GetResult();

    if (result != displayedEthTestResult)
    {
        updateEthTestResult(result);
    }
}

void screenView::ethTestButtonClicked(const touchgfx::ButtonWithLabel& source,
                                      const touchgfx::ClickEvent& event)
{
    (void)source;

    if (event.getType() == touchgfx::ClickEvent::RELEASED)
    {
        if (ETH_Loopback_RequestStart() != HAL_OK)
        {
            updateEthTestResult(ETH_TEST_FAULT);
        }
    }
}

void screenView::updateEthTestResult(ETH_TestResultTypeDef result)
{
    const char* text;

    switch (result)
    {
    case ETH_TEST_IDLE:
        text = "READY";
        break;
    case ETH_TEST_RUNNING:
        text = "RUNNING";
        break;
    case ETH_TEST_PASS:
        text = "PASS";
        break;
    case ETH_TEST_FAULT:
        text = "FAULT";
        break;
    default:
        text = "NOT READY";
        break;
    }

    touchgfx::Unicode::fromUTF8(reinterpret_cast<const uint8_t*>(text),
                               textArea2Buffer,
                               TEXTAREA2_SIZE);
    displayedEthTestResult = result;
    textArea2.invalidate();
}
