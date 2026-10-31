package com.vrlightgunhub.gunbladeny;

/** Runs the actual UI-thread policy; no Android or lifecycle mocks. */
public final class LauncherPolicyHarness {
    private static void expect(boolean value, String message) {
        if (!value) throw new AssertionError(message);
    }
    private static void checked(ReadyLaunchPolicy policy, boolean valid) {
        int token = policy.beginCheck();
        expect(token >= 0, "foreground validation begins");
        expect(policy.beginCheck() == -1, "pending validations cannot duplicate");
        expect(!policy.isReady(), "a fresh validation disables Play");
        expect(policy.finishCheck(token, valid), "current validation is accepted");
    }
    public static void main(String[] args) {
        ReadyLaunchPolicy p = new ReadyLaunchPolicy(false, false);
        expect(p.beginCheck() == -1, "background activity cannot validate or launch");
        p.onResume(); checked(p, false);
        expect(!p.isReady() && !p.shouldLaunch() && !p.requestPlay(), "invalid files never enter VR");
        checked(p, true);
        expect(p.shouldLaunch(), "validated owned ROMs automatically enter on first launch");
        expect(!p.shouldLaunch(), "automatic entry is consumed before handoff");
        expect(p.beginLaunch(), "foreground handoff begins");
        expect(!p.beginLaunch() && !p.requestPlay(), "handoff cannot duplicate");
        p.onNewEntry(false);
        expect(!p.shouldLaunch(), "a reused intent cannot duplicate a finishing handoff");

        for (boolean restored : new boolean[] {false, true}) {
            p = new ReadyLaunchPolicy(!restored, restored); p.onResume(); checked(p, true);
            expect(!p.shouldLaunch(), "returned and recreated consumed panels do not relaunch");
            expect(p.requestPlay(), "returned panel allows explicit Play");
            checked(p, true);
            expect(p.shouldLaunch() && p.beginLaunch(), "explicit Play validates again then enters");
            p.launchFailed();
            expect(!p.shouldLaunch() && p.requestPlay(), "failed handoff requires explicit retry");
        }

        p = new ReadyLaunchPolicy(false, false); p.onResume();
        int token = p.beginCheck(); p.onPause();
        expect(!p.finishCheck(token, true), "settings/background invalidates an in-flight folder check");
        expect(!p.shouldLaunch() && !p.beginLaunch(), "background completion cannot enter VR");
        p.onResume(); checked(p, true);
        expect(p.shouldLaunch() && p.beginLaunch(), "resume revalidates and enters only once");

        p = new ReadyLaunchPolicy(false, false); p.onResume(); token = p.beginCheck();
        p.onNewEntry(true);
        expect(!p.finishCheck(token, true), "stale result cannot override a returned entry");
        checked(p, true);
        expect(!p.shouldLaunch(), "returned new intent stays on the panel");
        p.onNewEntry(false); checked(p, true);
        expect(p.shouldLaunch() && p.autoStartConsumed(), "fresh library entry can auto launch");

        p = new ReadyLaunchPolicy(true, false); p.onResume(); checked(p, true);
        expect(p.requestPlay(), "manual request begins from ready state");
        checked(p, false);
        expect(!p.shouldLaunch() && !p.isReady(), "removing a ROM before Play blocks entry");
        checked(p, true); expect(p.requestPlay(), "ready again allows retry");
        p.onPause(); p.onResume(); checked(p, true);
        expect(!p.shouldLaunch(), "a manual request cannot survive a settings/background visit");
        System.out.println("PASS once-only automatic entry, return, recreation, pause, stale checks and retry");
    }
}
