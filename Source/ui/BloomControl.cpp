#include "BloomControl.h"
BloomControl::BloomControl() : BubbleKnob("%",false,1)
{
    setName("Bloom");
    setTooltip("Adds effective Humanize, Vibrato, Smoothness and up to +0.75 st formant character. Other controls show editable base values. No gain or Dry/Wet change.");
}
