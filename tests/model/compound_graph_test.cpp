#include <nxtcut/model/compound_graph.hpp>

#include <gtest/gtest.h>

#include <utility>
#include <vector>

#include <nxtcut_test/model_fixtures.hpp>

namespace nxtcut::model {
namespace {

TEST(CompoundGraphTest, SelfLoopCycleOfLengthOne) {
    core::UuidGenerator gen(1ULL);
    const SequenceId seq_a = generate_id<SequenceId>(gen);

    Project project;
    project.main_sequence = seq_a;
    project.sequences[seq_a] = test::make_compound_sequence(gen, seq_a, {seq_a});

    const auto cycles = find_compound_cycles(project);
    EXPECT_EQ(cycles.size(), 1U);

    const auto cycle = find_compound_cycle(project);
    ASSERT_TRUE(cycle.has_value());
    EXPECT_EQ(cycle->size(), 1U);
    EXPECT_EQ((*cycle)[0], seq_a);

    EXPECT_TRUE(would_create_cycle(project, seq_a, seq_a));
}

TEST(CompoundGraphTest, TwoCycleAtoBtoA) {
    core::UuidGenerator gen(2ULL);
    const SequenceId seq_a = generate_id<SequenceId>(gen);
    const SequenceId seq_b = generate_id<SequenceId>(gen);

    Project project;
    project.main_sequence = seq_a;
    project.sequences[seq_a] = test::make_compound_sequence(gen, seq_a, {seq_b});
    project.sequences[seq_b] = test::make_compound_sequence(gen, seq_b, {seq_a});

    const auto cycles = find_compound_cycles(project);
    EXPECT_EQ(cycles.size(), 1U);

    const auto cycle = find_compound_cycle(project);
    ASSERT_TRUE(cycle.has_value());
    EXPECT_EQ(cycle->size(), 2U);

    // If only A -> B existed, test would_create_cycle:
    Project partial = project;
    partial.sequences[seq_b].tracks[0].clips.clear();
    // In partial: A -> B exists. B containing A would create cycle!
    EXPECT_TRUE(would_create_cycle(partial, seq_b, seq_a));
    // A containing B already exists, but asking if B can contain B is true
    EXPECT_TRUE(would_create_cycle(partial, seq_b, seq_b));
}

TEST(CompoundGraphTest, ThreeCycleAtoBtoCtoA) {
    core::UuidGenerator gen(3ULL);
    const SequenceId seq_a = generate_id<SequenceId>(gen);
    const SequenceId seq_b = generate_id<SequenceId>(gen);
    const SequenceId seq_c = generate_id<SequenceId>(gen);

    Project project;
    project.main_sequence = seq_a;
    project.sequences[seq_a] = test::make_compound_sequence(gen, seq_a, {seq_b});
    project.sequences[seq_b] = test::make_compound_sequence(gen, seq_b, {seq_c});
    project.sequences[seq_c] = test::make_compound_sequence(gen, seq_c, {seq_a});

    const auto cycles = find_compound_cycles(project);
    EXPECT_EQ(cycles.size(), 1U);

    const auto cycle = find_compound_cycle(project);
    ASSERT_TRUE(cycle.has_value());
    EXPECT_EQ(cycle->size(), 3U);
}

TEST(CompoundGraphTest, DiamondIsAcyclic) {
    // Diamond: A -> B, A -> C, B -> D, C -> D
    core::UuidGenerator gen(4ULL);
    const SequenceId seq_a = generate_id<SequenceId>(gen);
    const SequenceId seq_b = generate_id<SequenceId>(gen);
    const SequenceId seq_c = generate_id<SequenceId>(gen);
    const SequenceId seq_d = generate_id<SequenceId>(gen);

    Project project;
    project.main_sequence = seq_a;

    Sequence a;
    a.id = seq_a;
    Track t_a;
    Clip c_ab;
    c_ab.content = CompoundContent{seq_b};
    Clip c_ac;
    c_ac.content = CompoundContent{seq_c};
    t_a.clips.push_back(c_ab);
    t_a.clips.push_back(c_ac);
    a.tracks.push_back(t_a);
    project.sequences[seq_a] = a;

    Sequence b;
    b.id = seq_b;
    Track t_b;
    Clip c_bd;
    c_bd.content = CompoundContent{seq_d};
    t_b.clips.push_back(c_bd);
    b.tracks.push_back(t_b);
    project.sequences[seq_b] = b;

    Sequence c;
    c.id = seq_c;
    Track t_c;
    Clip c_cd;
    c_cd.content = CompoundContent{seq_d};
    t_c.clips.push_back(c_cd);
    c.tracks.push_back(t_c);
    project.sequences[seq_c] = c;

    Sequence d;
    d.id = seq_d;
    project.sequences[seq_d] = d;

    const auto cycles = find_compound_cycles(project);
    EXPECT_TRUE(cycles.empty());

    const auto cycle = find_compound_cycle(project);
    EXPECT_FALSE(cycle.has_value());

    // D containing A would create cycle
    EXPECT_TRUE(would_create_cycle(project, seq_d, seq_a));
    // D containing B would create cycle
    EXPECT_TRUE(would_create_cycle(project, seq_d, seq_b));
    // B containing C would NOT create cycle
    EXPECT_FALSE(would_create_cycle(project, seq_b, seq_c));
}

TEST(CompoundGraphTest, TwoDisjointCyclesProduceTwoCycles) {
    core::UuidGenerator gen(20ULL);
    const SequenceId seq_a = generate_id<SequenceId>(gen);
    const SequenceId seq_b = generate_id<SequenceId>(gen);
    const SequenceId seq_c = generate_id<SequenceId>(gen);
    const SequenceId seq_d = generate_id<SequenceId>(gen);

    Project project;
    project.main_sequence = seq_a;
    project.sequences[seq_a] = test::make_compound_sequence(gen, seq_a, {seq_b});
    project.sequences[seq_b] = test::make_compound_sequence(gen, seq_b, {seq_a});
    project.sequences[seq_c] = test::make_compound_sequence(gen, seq_c, {seq_d});
    project.sequences[seq_d] = test::make_compound_sequence(gen, seq_d, {seq_c});

    const auto cycles = find_compound_cycles(project);
    EXPECT_EQ(cycles.size(), 2U);
}

TEST(CompoundGraphTest, BranchingCyclesShareRootProduceTwoCycles) {
    // A -> B -> A and A -> C -> A
    core::UuidGenerator gen(21ULL);
    const SequenceId seq_a = generate_id<SequenceId>(gen);
    const SequenceId seq_b = generate_id<SequenceId>(gen);
    const SequenceId seq_c = generate_id<SequenceId>(gen);

    Project project;
    project.main_sequence = seq_a;
    project.sequences[seq_a] = test::make_compound_sequence(gen, seq_a, {seq_b, seq_c});
    project.sequences[seq_b] = test::make_compound_sequence(gen, seq_b, {seq_a});
    project.sequences[seq_c] = test::make_compound_sequence(gen, seq_c, {seq_a});

    const auto cycles = find_compound_cycles(project);
    EXPECT_EQ(cycles.size(), 2U);
}

TEST(CompoundGraphTest, ParallelEdgesProduceSingleCycle) {
    // B holds TWO compound clips that both point at A, with A pointing at B
    core::UuidGenerator gen(22ULL);
    const SequenceId seq_a = generate_id<SequenceId>(gen);
    const SequenceId seq_b = generate_id<SequenceId>(gen);

    Project project;
    project.main_sequence = seq_a;
    project.sequences[seq_a] = test::make_compound_sequence(gen, seq_a, {seq_b});
    project.sequences[seq_b] = test::make_compound_sequence(gen, seq_b, {seq_a, seq_a});

    const auto cycles = find_compound_cycles(project);
    EXPECT_EQ(cycles.size(), 1U);
}

TEST(CompoundGraphTest, LongChainTenThousandSequencesNoCycleNoOverflow) {
    core::UuidGenerator gen(5ULL);
    Project project;

    constexpr int kCount = 10000;
    std::vector<SequenceId> ids;
    ids.reserve(kCount);
    for (int i = 0; i < kCount; ++i) {
        ids.push_back(generate_id<SequenceId>(gen));
    }

    project.main_sequence = ids[0];

    for (int i = 0; i < kCount; ++i) {
        Sequence s;
        s.id = ids[static_cast<std::size_t>(i)];
        if (i + 1 < kCount) {
            Track t;
            Clip c;
            c.content = CompoundContent{ids[static_cast<std::size_t>(i + 1)]};
            t.clips.push_back(c);
            s.tracks.push_back(t);
        }
        project.sequences[s.id] = s;
    }

    const auto cycles = find_compound_cycles(project);
    EXPECT_TRUE(cycles.empty());

    const auto cycle = find_compound_cycle(project);
    EXPECT_FALSE(cycle.has_value());

    // Last element containing first would create cycle
    EXPECT_TRUE(would_create_cycle(project, ids[kCount - 1], ids[0]));
    // First element containing non-existent sequence or unrelated does not
    const SequenceId unrelated = generate_id<SequenceId>(gen);
    EXPECT_FALSE(would_create_cycle(project, ids[0], unrelated));
}

TEST(CompoundGraphTest, CycleNotIncludingMainSequenceIsFound) {
    core::UuidGenerator gen(6ULL);
    const SequenceId main_seq = generate_id<SequenceId>(gen);
    const SequenceId seq_x = generate_id<SequenceId>(gen);
    const SequenceId seq_y = generate_id<SequenceId>(gen);

    Project project;
    project.main_sequence = main_seq;

    Sequence m;
    m.id = main_seq;
    project.sequences[main_seq] = m;
    project.sequences[seq_x] = test::make_compound_sequence(gen, seq_x, {seq_y});
    project.sequences[seq_y] = test::make_compound_sequence(gen, seq_y, {seq_x});

    const auto cycle = find_compound_cycle(project);
    ASSERT_TRUE(cycle.has_value());
    EXPECT_EQ(cycle->size(), 2U);
}

TEST(CompoundGraphTest, MissingSequenceIsIgnoredInCycleDetection) {
    core::UuidGenerator gen(7ULL);
    const SequenceId seq_a = generate_id<SequenceId>(gen);
    const SequenceId missing = generate_id<SequenceId>(gen);

    Project project;
    project.main_sequence = seq_a;
    project.sequences[seq_a] = test::make_compound_sequence(gen, seq_a, {missing});

    const auto cycle = find_compound_cycle(project);
    EXPECT_FALSE(cycle.has_value());
}

TEST(CompoundGraphTest, WouldCreateCycleDirectAndReachabilityChecks) {
    core::UuidGenerator gen(8ULL);
    const SequenceId seq_a = generate_id<SequenceId>(gen);
    const SequenceId seq_b = generate_id<SequenceId>(gen);
    const SequenceId seq_c = generate_id<SequenceId>(gen);

    Project project;
    project.main_sequence = seq_a;

    // A -> B exists
    Sequence a;
    a.id = seq_a;
    Track t_a;
    Clip c_ab;
    c_ab.content = CompoundContent{seq_b};
    t_a.clips.push_back(c_ab);
    a.tracks.push_back(t_a);
    project.sequences[seq_a] = a;

    Sequence b;
    b.id = seq_b;
    project.sequences[seq_b] = b;

    Sequence c;
    c.id = seq_c;
    project.sequences[seq_c] = c;

    // A to B exists: asking whether B may contain A returns true
    EXPECT_TRUE(would_create_cycle(project, seq_b, seq_a));
    // Asking whether A may contain C returns false
    EXPECT_FALSE(would_create_cycle(project, seq_a, seq_c));
}

}  // namespace
}  // namespace nxtcut::model
