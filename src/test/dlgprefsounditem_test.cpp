#include "preferences/dialog/dlgprefsounditem.h"

#include <gtest/gtest.h>

#include <QString>

#include "soundio/soundmanagerutil.h"
#include "test/mixxxtest.h"

namespace {

using AccessibleNames = DlgPrefSoundItem::AccessibleNames;

} // namespace

class DlgPrefSoundItemTest : public MixxxTest {
};

// ---------------------------------------------------------------------------
// accessibleNamesFor
// ---------------------------------------------------------------------------

TEST_F(DlgPrefSoundItemTest, MainOutput_DeviceAndChannelNames) {
    const AccessibleNames names =
            DlgPrefSoundItem::accessibleNamesFor(AudioPathType::Main, 0, false);
    EXPECT_QSTRING_EQ("Main output device", names.device);
    EXPECT_QSTRING_EQ("Main output channel", names.channel);
}

TEST_F(DlgPrefSoundItemTest, HeadphonesOutput_DeviceAndChannelNames) {
    const AccessibleNames names =
            DlgPrefSoundItem::accessibleNamesFor(AudioPathType::Headphones, 0, false);
    EXPECT_QSTRING_EQ("Headphones output device", names.device);
    EXPECT_QSTRING_EQ("Headphones output channel", names.channel);
}

TEST_F(DlgPrefSoundItemTest, MicrophoneInput_UsesInputQualifier) {
    const AccessibleNames names =
            DlgPrefSoundItem::accessibleNamesFor(AudioPathType::Microphone, 0, true);
    EXPECT_QSTRING_EQ("Microphone 1 input device", names.device);
    EXPECT_QSTRING_EQ("Microphone 1 input channel", names.channel);
}

TEST_F(DlgPrefSoundItemTest, DeckOutput_IndexedNames) {
    const AccessibleNames names =
            DlgPrefSoundItem::accessibleNamesFor(AudioPathType::Deck, 2, false);
    EXPECT_QSTRING_EQ("Deck 3 output device", names.device);
    EXPECT_QSTRING_EQ("Deck 3 output channel", names.channel);
}

TEST_F(DlgPrefSoundItemTest, SamplerOutput_IndexedNames) {
    // Samplers are represented as Deck paths with a high index.
    const AccessibleNames names =
            DlgPrefSoundItem::accessibleNamesFor(AudioPathType::Deck, 3, false);
    EXPECT_QSTRING_EQ("Deck 4 output device", names.device);
    EXPECT_QSTRING_EQ("Deck 4 output channel", names.channel);
}

TEST_F(DlgPrefSoundItemTest, BoothOutput_DistinctFromMain) {
    const AccessibleNames booth =
            DlgPrefSoundItem::accessibleNamesFor(AudioPathType::Booth, 0, false);
    const AccessibleNames main =
            DlgPrefSoundItem::accessibleNamesFor(AudioPathType::Main, 0, false);
    EXPECT_NE(booth.device, main.device);
    EXPECT_NE(booth.channel, main.channel);
    EXPECT_QSTRING_EQ("Booth output device", booth.device);
    EXPECT_QSTRING_EQ("Booth output channel", booth.channel);
}

TEST_F(DlgPrefSoundItemTest, VinylControlOutput_DistinctNames) {
    const AccessibleNames names =
            DlgPrefSoundItem::accessibleNamesFor(AudioPathType::VinylControl, 0, false);
    EXPECT_QSTRING_EQ("Vinyl Control 1 output device", names.device);
    EXPECT_QSTRING_EQ("Vinyl Control 1 output channel", names.channel);
}

TEST_F(DlgPrefSoundItemTest, IndexedInput_IndexIncluded) {
    const AccessibleNames names =
            DlgPrefSoundItem::accessibleNamesFor(AudioPathType::Auxiliary, 1, true);
    EXPECT_QSTRING_EQ("Auxiliary 2 input device", names.device);
    EXPECT_QSTRING_EQ("Auxiliary 2 input channel", names.channel);
}

TEST_F(DlgPrefSoundItemTest, SameTypeDifferentIndex_ProducesDistinctNames) {
    const AccessibleNames deck1 =
            DlgPrefSoundItem::accessibleNamesFor(AudioPathType::Deck, 0, false);
    const AccessibleNames deck2 =
            DlgPrefSoundItem::accessibleNamesFor(AudioPathType::Deck, 1, false);
    EXPECT_NE(deck1.device, deck2.device);
    EXPECT_NE(deck1.channel, deck2.channel);
}
