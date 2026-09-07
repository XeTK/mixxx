#import <CoreAudioKit/CoreAudioKit.h>
#import <UIKit/UIKit.h>
#import <objc/runtime.h>

#include "controllers/midi/blemidipairingios.h"

#include <utility>

namespace {
mixxx::ios::BluetoothPairingDismissedReceiver s_dismissedReceiver;
} // namespace

// Owns the "Done" button's target-action callback and dismisses the
// presented navigation controller. Block-based UIBarButtonItem actions
// need iOS 14+; a plain target-action object works on whatever minimum iOS
// version Mixxx otherwise targets.
@interface MixxxBleMidiPairingDoneHandler : NSObject
@property(nonatomic, weak) UIViewController* presentingController;
- (void)done;
@end

@implementation MixxxBleMidiPairingDoneHandler
- (void)done {
    [self.presentingController
            dismissViewControllerAnimated:YES
                                completion:^{
                                    if (s_dismissedReceiver) {
                                        s_dismissedReceiver();
                                    }
                                }];
}
@end

namespace mixxx {
namespace ios {

void setBluetoothPairingDismissedReceiver(BluetoothPairingDismissedReceiver receiver) {
    s_dismissedReceiver = std::move(receiver);
}

bool presentBluetoothMidiPairingUI() {
    // Qt's iOS platform plugin creates exactly one UIWindowScene/UIWindow
    // for the app; walk the scenes rather than assuming a single global
    // window to stay correct if that ever changes (e.g. multi-window
    // iPadOS support).
    UIViewController* rootViewController = nil;
    for (UIScene* scene in UIApplication.sharedApplication.connectedScenes) {
        if (![scene isKindOfClass:[UIWindowScene class]]) {
            continue;
        }
        UIWindowScene* windowScene = (UIWindowScene*)scene;
        for (UIWindow* window in windowScene.windows) {
            if (window.isKeyWindow) {
                rootViewController = window.rootViewController;
                break;
            }
        }
        if (rootViewController) {
            break;
        }
    }
    if (!rootViewController) {
        return false;
    }
    // Present over whatever's already on top (e.g. a preferences dialog),
    // not necessarily the bare root - UIKit forbids presenting a second
    // modal directly on a view controller that's already presenting one.
    UIViewController* topController = rootViewController;
    while (topController.presentedViewController) {
        topController = topController.presentedViewController;
    }

    CABTMIDICentralViewController* pairingController =
            [[CABTMIDICentralViewController alloc] init];
    pairingController.navigationItem.title = @"Bluetooth MIDI Devices";

    MixxxBleMidiPairingDoneHandler* doneHandler = [[MixxxBleMidiPairingDoneHandler alloc] init];
    doneHandler.presentingController = topController;
    pairingController.navigationItem.rightBarButtonItem = [[UIBarButtonItem alloc]
            initWithBarButtonSystemItem:UIBarButtonSystemItemDone
                                  target:doneHandler
                                  action:@selector(done)];
    // pairingController doesn't otherwise hold a strong reference to
    // doneHandler (navigationItem.rightBarButtonItem's target is weak/
    // unretained on some UIKit versions) - an associated object keeps it
    // alive for exactly the presented controller's lifetime.
    objc_setAssociatedObject(pairingController,
            "mixxxBleMidiPairingDoneHandler",
            doneHandler,
            OBJC_ASSOCIATION_RETAIN);

    UINavigationController* navController =
            [[UINavigationController alloc] initWithRootViewController:pairingController];
    navController.modalPresentationStyle = UIModalPresentationFormSheet;
    [topController presentViewController:navController animated:YES completion:nil];
    return true;
}

} // namespace ios
} // namespace mixxx
