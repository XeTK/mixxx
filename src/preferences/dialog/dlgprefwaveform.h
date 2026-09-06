#pragma once

#include <memory>

#include "preferences/dialog/dlgpreferencepage.h"
#include "preferences/dialog/ui_dlgprefwaveformdlg.h"
#include "preferences/usersettings.h"
#include "waveform/widgets/waveformwidgettype.h"
// allshader::WaveformRendererSignalBase::Options is used unconditionally
// below (updateWaveformTypeOptions()) regardless of which allshader backend
// is active - mixxx-lib always has the `allshader` macro resolved to a real
// namespace (allshader_gl where Qt6::OpenGL exists, allshader_sg on iOS -
// see the rendergraph section of the top-level CMakeLists.txt), so this
// doesn't need to be conditional on MIXXX_USE_QOPENGL specifically (that
// was only ever a proxy for "is any allshader backend available", which is
// no longer 1:1 now that iOS has one - allshader_sg - without the other).
#include "waveform/renderers/allshader/waveformrenderersignalbase.h"

class ControlPushButton;
class ControlObject;
class Library;

class DlgPrefWaveform : public DlgPreferencePage, public Ui::DlgPrefWaveformDlg {
    Q_OBJECT
  public:
    DlgPrefWaveform(
            QWidget* pParent,
            UserSettingsPointer pConfig,
            std::shared_ptr<Library> pLibrary);
    virtual ~DlgPrefWaveform();

  public slots:
    void slotUpdate() override;
    void slotApply() override;
    void slotResetToDefaults() override;
    void slotSetWaveformEndRender(int endTime);

  private slots:
    void slotSetFrameRate(int frameRate);
    void slotSetWaveformType(int index);
    void slotSetWaveformEnabled(bool checked);
    void slotSetWaveformAcceleration(bool checked);
    void slotSetWaveformOptions(allshader::WaveformRendererSignalBase::Option option, bool enabled);
    void slotSetWaveformOptionSplitStereoSignal(bool checked) {
        slotSetWaveformOptions(allshader::WaveformRendererSignalBase::Option::
                                       SplitStereoSignal,
                checked);
    }
    void slotSetWaveformOptionHighDetail(bool checked) {
        slotSetWaveformOptions(allshader::WaveformRendererSignalBase::Option::HighDetail, checked);
    }
    void slotSetDefaultZoom(int index);
    void slotSetZoomSynchronization(bool checked);
    void slotSetVisualGainAll(double gain);
    void slotSetVisualGainLow(double gain);
    void slotSetVisualGainMid(double gain);
    void slotSetVisualGainHigh(double gain);
    void slotWaveformMeasured(float frameRate, int droppedFrames);
    void slotClearCachedWaveforms();
    void slotSetBeatGridAlpha(int alpha);
    void slotSetPlayMarkerPosition(int position);
    void slotSetUntilMarkShowBeats(bool checked);
    void slotSetUntilMarkShowTime(bool checked);
    void slotSetUntilMarkAlign(int index);
    void slotSetUntilMarkTextPointSize(int value);
    void slotSetUntilMarkTextHeightLimit(int index);
    void slotStemOpacity(float value);
    void slotStemReorderOnChange(bool value);
    void slotStemOutlineOpacity(float value);
    void slotStemDisplayMode(int index);
    // overview options
    void slotSetWaveformOverviewType();
    void slotSetOverviewMinuteMarkers(bool minuteMarkers);
    void slotSetOverviewScaling();

  private:
    void initWaveformControl();
    void calculateCachedWaveformDiskUsage();
    void notifyRebootNecessary();
    void updateEnableUntilMark();
    void updateWaveformTypeOptions(bool useWaveform,
            WaveformWidgetBackend backend,
            allshader::WaveformRendererSignalBase::Options currentOption);
    void updateWaveformAcceleration(
            WaveformWidgetType::Type type, WaveformWidgetBackend backend);
    void updateWaveformGeneralOptionsEnabled();
    void updateWaveformGainEnabled();
    void updateStemOptionsEnabled();

    std::unique_ptr<ControlPushButton> m_pTypeControl;
    std::unique_ptr<ControlObject> m_pOverviewMinuteMarkersControl;

    UserSettingsPointer m_pConfig;
    std::shared_ptr<Library> m_pLibrary;
};
