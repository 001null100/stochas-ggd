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

    juce::StringArray categories;
    if (args.isEmpty())
    {
        categories.add("ggd");
       #if ! STOCHAS_CORE_TESTS_ONLY
        categories.add("ggd-plugin");
       #endif
    }
    else if (!args.contains("--all"))
    {
        categories = args;
    }

    // Every run clears the runner's results, so count after each one.
    int failures = 0;
    int passes = 0;
    juce::UnitTestRunner runner;
    runner.setAssertOnFailure(false);
    runner.setPassesAreLogged(false);
    auto tally = [&]
    {
        for (int i = 0; i < runner.getNumResults(); ++i)
        {
            failures += runner.getResult(i)->failures;
            passes += runner.getResult(i)->passes;
        }
    };

    if (categories.isEmpty())
    {
        runner.runAllTests();
        tally();
    }
    else
    {
        for (const auto& category : categories)
        {
            runner.runTestsInCategory(category);
            tally();
        }
    }

    std::printf("\n%d checks passed, %d failed\n", passes, failures);
    return failures > 0 ? 1 : 0;
}
