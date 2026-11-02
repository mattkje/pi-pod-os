#include "ui/pages/ComingSoonPage.h"
#include "app/I18n.h"

void drawComingSoonPage(const UI &ui, const std::string &title) {
    drawTopBar(ui, tr(title));
    int y = ui.h / 2 - 20;
    drawText(ui.r, ui.font, tr("Coming soon"), TEXT_COLOR, ui.w / 2, y, 1);
    drawText(ui.r, ui.small, trf("{} isn't available yet", {tr(title)}), SUBTEXT_COLOR, ui.w / 2, y + 26, 1, ui.w - 20);
}
