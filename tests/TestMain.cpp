#include <JuceHeader.h>

#include <cstdio>

#if ! STOCHAS_CORE_TESTS_ONLY
// PluginTests.cpp: offscreen editor snapshots for visual review.
int runSnapshotTool(const juce::StringArray& args);
#endif

int main(int argc, char* argv[])
{
    juce::StringArray args;
    for (int i = 1; i < argc; ++i)
        args.add(argv[i]);

#if ! STOCHAS_CORE_TESTS_ONLY
    juce::ScopedJuceInitialiser_GUI gui;
    if (args.contains("--snapshot"))
        return runSnapshotTool(args);
#endif

    juce::UnitTestRunner runner;
    runner.setAssertOnFailure(false);
    runner.setPassesAreLogged(false);

    if (args.isEmpty())
    {
        runner.runTestsInCategory("ggd");
       #if ! STOCHAS_CORE_TESTS_ONLY
        runner.runTestsInCategory("ggd-plugin");
       #endif
    }
    else if (args.contains("--all"))
        runner.runAllTests();
    else
        for (const auto& category : args)
            runner.runTestsInCategory(category);

    int failures = 0;
    int passes = 0;
    for (int i = 0; i < runner.getNumResults(); ++i)
    {
        failures += runner.getResult(i)->failures;
        passes += runner.getResult(i)->passes;
    }

    std::printf("\n%d checks passed, %d failed\n", passes, failures);
    return failures > 0 ? 1 : 0;
}
