#include "VocalisLogo.h"
#include "VocalisTheme.h"
#include <BinaryData.h>

VocalisLogo::VocalisLogo()
    : image(juce::ImageFileFormat::loadFrom(BinaryData::VocalisLogo_png,
                                          BinaryData::VocalisLogo_pngSize))
{
    setName("Vocalis logo");
    setInterceptsMouseClicks(false, false);
}

void VocalisLogo::paint(juce::Graphics& g)
{
    auto area = getLocalBounds().toFloat();
    auto subtitle = area.removeFromBottom(26);
    area.removeFromBottom(8);
    if (image.isValid())
    {
        const juce::RectanglePlacement placement(juce::RectanglePlacement::centred
                                                 | juce::RectanglePlacement::onlyReduceInSize);
        const auto fitted = placement.appliedTo(image.getBounds().toFloat(), area);
        // The supplied artwork has an opaque background. Round its fitted edge
        // without stretching/cropping the lettering or adding a rectangular frame.
        juce::Graphics::ScopedSaveState saved(g);
        juce::Path outline;
        outline.addRoundedRectangle(fitted, VocalisTheme::corner);
        g.reduceClipRegion(outline);
        g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);
        g.drawImage(image, fitted);
    }
    else
    {
        g.setColour(VocalisTheme::ink);
        g.setFont(VocalisTheme::font(VocalisTheme::brandType, true));
        g.drawText("VOCALIS", area, juce::Justification::centred);
    }
    g.setColour(VocalisTheme::muted);
    g.setFont(VocalisTheme::font(16));
    g.drawText("Vocal Shaping Engine", subtitle, juce::Justification::centred);
}
