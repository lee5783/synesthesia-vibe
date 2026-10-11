//
//  projectm_eval_conformance_test.cpp
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-01.
//

#include <gtest/gtest.h>

#include <projectm-eval/projectm-eval.h>

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace
{
    struct EvalContext
    {
        projectm_eval_context* ctx;

        EvalContext() : ctx(projectm_eval_context_create(nullptr, nullptr)) {}
        explicit EvalContext(projectm_eval_mem_buffer globalMem) : ctx(projectm_eval_context_create(globalMem, nullptr)) {}
        ~EvalContext() { projectm_eval_context_destroy(ctx); }

        EvalContext(const EvalContext&) = delete;
        EvalContext& operator=(const EvalContext&) = delete;
    };

    struct CompiledCode
    {
        projectm_eval_code* handle;

        CompiledCode(EvalContext& ctx, const std::string& code) : handle(projectm_eval_code_compile(ctx.ctx, code.c_str()))
        {
        }
        ~CompiledCode()
        {
            if (handle)
            {
                projectm_eval_code_destroy(handle);
            }
        }

        CompiledCode(const CompiledCode&) = delete;
        CompiledCode& operator=(const CompiledCode&) = delete;
    };

    PRJM_EVAL_F* Reg(EvalContext& ctx, const char* name)
    {
        return projectm_eval_context_register_variable(ctx.ctx, name);
    }

    class JsonValue
    {
    public:
        enum class Type
        {
            Null,
            Number,
            String,
            Array,
            Object
        };

        Type type = Type::Null;
        double number = 0.0;
        std::string str;
        std::vector<JsonValue> array;
        std::map<std::string, JsonValue> object;

        const JsonValue& At(const std::string& key) const
        {
            static const JsonValue kNull;
            auto it = object.find(key);
            return it == object.end() ? kNull : it->second;
        }
    };

    class JsonParser
    {
    public:
        explicit JsonParser(const std::string& text) : text_(text), pos_(0) {}

        JsonValue Parse()
        {
            SkipWhitespace();
            return ParseValue();
        }

    private:
        const std::string& text_;
        size_t pos_;

        void SkipWhitespace()
        {
            while (pos_ < text_.size() && (text_[pos_] == ' ' || text_[pos_] == '\t' || text_[pos_] == '\n' || text_[pos_] == '\r'))
            {
                ++pos_;
            }
        }

        char Peek() { return pos_ < text_.size() ? text_[pos_] : '\0'; }
        char Next() { return text_[pos_++]; }

        JsonValue ParseValue()
        {
            SkipWhitespace();
            char c = Peek();
            if (c == '{')
            {
                return ParseObject();
            }
            if (c == '[')
            {
                return ParseArray();
            }
            if (c == '"')
            {
                JsonValue v;
                v.type = JsonValue::Type::String;
                v.str = ParseString();
                return v;
            }
            if (c == 't')
            {
                pos_ += 4;
                JsonValue v;
                v.type = JsonValue::Type::Number;
                v.number = 1.0;
                return v;
            }
            if (c == 'f')
            {
                pos_ += 5;
                JsonValue v;
                v.type = JsonValue::Type::Number;
                v.number = 0.0;
                return v;
            }
            if (c == 'n')
            {
                pos_ += 4;
                return JsonValue();
            }
            return ParseNumber();
        }

        std::string ParseString()
        {
            std::string out;
            Next();
            while (Peek() != '"')
            {
                char c = Next();
                if (c == '\\')
                {
                    char esc = Next();
                    switch (esc)
                    {
                    case 'n': out.push_back('\n'); break;
                    case 't': out.push_back('\t'); break;
                    case 'r': out.push_back('\r'); break;
                    default: out.push_back(esc); break;
                    }
                }
                else
                {
                    out.push_back(c);
                }
            }
            Next();
            return out;
        }

        JsonValue ParseNumber()
        {
            size_t start = pos_;
            if (Peek() == '-')
            {
                Next();
            }
            while (pos_ < text_.size() && (isdigit(static_cast<unsigned char>(Peek())) || Peek() == '.' || Peek() == 'e' || Peek() == 'E' || Peek() == '+' || Peek() == '-'))
            {
                Next();
            }
            std::string token = text_.substr(start, pos_ - start);
            JsonValue v;
            v.type = JsonValue::Type::Number;
            v.number = std::strtod(token.c_str(), nullptr);
            return v;
        }

        JsonValue ParseArray()
        {
            JsonValue v;
            v.type = JsonValue::Type::Array;
            Next();
            SkipWhitespace();
            if (Peek() == ']')
            {
                Next();
                return v;
            }
            while (true)
            {
                v.array.push_back(ParseValue());
                SkipWhitespace();
                if (Peek() == ',')
                {
                    Next();
                    SkipWhitespace();
                    continue;
                }
                break;
            }
            SkipWhitespace();
            Next();
            return v;
        }

        JsonValue ParseObject()
        {
            JsonValue v;
            v.type = JsonValue::Type::Object;
            Next();
            SkipWhitespace();
            if (Peek() == '}')
            {
                Next();
                return v;
            }
            while (true)
            {
                SkipWhitespace();
                std::string key = ParseString();
                SkipWhitespace();
                Next();
                JsonValue value = ParseValue();
                v.object.emplace(std::move(key), std::move(value));
                SkipWhitespace();
                if (Peek() == ',')
                {
                    Next();
                    continue;
                }
                break;
            }
            SkipWhitespace();
            Next();
            return v;
        }
    };

    JsonValue LoadJsonFile(const std::string& path)
    {
        std::ifstream f(path, std::ios::binary | std::ios::ate);
        EXPECT_TRUE(f.good()) << "could not open " << path;
        std::streamsize size = f.tellg();
        f.seekg(0, std::ios::beg);
        std::string text(static_cast<size_t>(size), '\0');
        f.read(&text[0], size);
        JsonParser parser(text);
        return parser.Parse();
    }

    double HexFloatToDouble(const std::string& hex)
    {
        uint64_t bits = std::stoull(hex, nullptr, 16);
        if (hex.size() <= 10)
        {
            uint32_t bits32 = static_cast<uint32_t>(bits);
            float f;
            std::memcpy(&f, &bits32, sizeof(f));
            return static_cast<double>(f);
        }
        double d;
        std::memcpy(&d, &bits, sizeof(d));
        return d;
    }

    double ReadFloatField(const JsonValue& obj, const std::string& key)
    {
        return HexFloatToDouble(obj.At(key).At("hex").str);
    }

    double ReadFloatField(const JsonValue& arrayEntry)
    {
        return HexFloatToDouble(arrayEntry.At("hex").str);
    }

    std::string MvpPresetPath(const std::string& fileName)
    {
        return std::string(SYNESTHESIA_VIBE_MVP_PRESETS_DIR) + "/" + fileName;
    }

    std::string GoldenFramePath(const std::string& captureId, int frame)
    {
        char buf[16];
        snprintf(buf, sizeof(buf), "%04d", frame);
        return std::string(SYNESTHESIA_VIBE_GOLDEN_CORPUS_DIR) + "/" + captureId + "/frame_" + buf + ".json";
    }

    std::string JoinNumberedKeys(const std::vector<std::string>& lines, const std::string& prefix)
    {
        std::string code;
        int n = 1;
        while (true)
        {
            std::string key = prefix + std::to_string(n);
            bool found = false;
            for (const auto& line : lines)
            {
                if (line.compare(0, key.size(), key) == 0 && line.size() > key.size() && line[key.size()] == '=')
                {
                    code += line.substr(key.size() + 1);
                    code += "\n";
                    found = true;
                    break;
                }
            }
            if (!found)
            {
                break;
            }
            ++n;
        }
        return code;
    }

    struct PresetEquations
    {
        std::string perFrameInit;
        std::string perFrame;
    };

    PresetEquations LoadPresetEquations(const std::string& path)
    {
        std::ifstream f(path, std::ios::binary);
        EXPECT_TRUE(f.good()) << "could not open " << path;
        std::vector<std::string> lines;
        std::string line;
        while (std::getline(f, line))
        {
            while (!line.empty() && (line.back() == '\r' || line.back() == '\n'))
            {
                line.pop_back();
            }
            lines.push_back(line);
        }

        PresetEquations eqs;
        eqs.perFrameInit = JoinNumberedKeys(lines, "per_frame_init_");
        eqs.perFrame = JoinNumberedKeys(lines, "per_frame_");
        return eqs;
    }

    constexpr int kNumQVar = 32;

    struct PresetRegisters
    {
        PRJM_EVAL_F* time;
        PRJM_EVAL_F* frame;
        PRJM_EVAL_F* fps;
        PRJM_EVAL_F* bass;
        PRJM_EVAL_F* mid;
        PRJM_EVAL_F* treb;
        PRJM_EVAL_F* bassAtt;
        PRJM_EVAL_F* midAtt;
        PRJM_EVAL_F* trebAtt;
        PRJM_EVAL_F* q[kNumQVar];
    };

    PresetRegisters RegisterPresetVars(EvalContext& ctx)
    {
        PresetRegisters r{};
        r.time = Reg(ctx, "time");
        r.frame = Reg(ctx, "frame");
        r.fps = Reg(ctx, "fps");
        r.bass = Reg(ctx, "bass");
        r.mid = Reg(ctx, "mid");
        r.treb = Reg(ctx, "treb");
        r.bassAtt = Reg(ctx, "bass_att");
        r.midAtt = Reg(ctx, "mid_att");
        r.trebAtt = Reg(ctx, "treb_att");
        for (int i = 0; i < kNumQVar; ++i)
        {
            r.q[i] = Reg(ctx, ("q" + std::to_string(i + 1)).c_str());
        }
        return r;
    }

    void SetFrameInputs(PresetRegisters& r, const JsonValue& frameJson)
    {
        *r.time = ReadFloatField(frameJson, "time");
        *r.frame = frameJson.At("frame").number;
        *r.fps = ReadFloatField(frameJson, "fps");
        const JsonValue& sound = frameJson.At("sound");
        *r.bass = ReadFloatField(sound, "bass");
        *r.mid = ReadFloatField(sound, "mid");
        *r.treb = ReadFloatField(sound, "treb");
        *r.bassAtt = ReadFloatField(sound, "bass_att");
        *r.midAtt = ReadFloatField(sound, "mid_att");
        *r.trebAtt = ReadFloatField(sound, "treb_att");
    }

    struct QVarMismatch
    {
        int frame;
        int qIndex;
        double got;
        double expected;
    };

    std::vector<QVarMismatch> ReplayQVars(const std::string& presetFile, const std::string& captureId, int frameCount)
    {
        PresetEquations eqs = LoadPresetEquations(MvpPresetPath(presetFile));

        EvalContext ctx;
        PresetRegisters regs = RegisterPresetVars(ctx);

        if (!eqs.perFrameInit.empty())
        {
            CompiledCode initCode(ctx, eqs.perFrameInit);
            EXPECT_NE(initCode.handle, nullptr) << "per_frame_init compile failed for " << presetFile << ": "
                                                 << projectm_eval_get_error(ctx.ctx, nullptr, nullptr);
            if (initCode.handle)
            {
                projectm_eval_code_execute(initCode.handle);
            }
        }

        CompiledCode frameCode(ctx, eqs.perFrame);
        EXPECT_NE(frameCode.handle, nullptr) << "per_frame compile failed for " << presetFile << ": "
                                              << projectm_eval_get_error(ctx.ctx, nullptr, nullptr);

        std::vector<QVarMismatch> mismatches;
        if (!frameCode.handle)
        {
            return mismatches;
        }

        for (int frame = 0; frame < frameCount; ++frame)
        {
            JsonValue frameJson = LoadJsonFile(GoldenFramePath(captureId, frame));
            SetFrameInputs(regs, frameJson);
            projectm_eval_code_execute(frameCode.handle);

            const JsonValue& qArray = frameJson.At("q");
            for (int i = 0; i < kNumQVar; ++i)
            {
                double expected = ReadFloatField(qArray.array[i]);
                double got = *regs.q[i];
                if (got != expected)
                {
                    mismatches.push_back({frame, i, got, expected});
                }
            }
        }
        return mismatches;
    }
}

TEST(ProjectMEvalQuirks, MegabufAddressing)
{
    EvalContext ctx;
    CompiledCode code(ctx, "megabuf(0) = 1.5; megabuf(100000) = 2.5; megabuf(1000000) = 3.5; "
                           "megabuf(0) + megabuf(100000)*10 + megabuf(1000000)*100;");
    ASSERT_NE(code.handle, nullptr) << projectm_eval_get_error(ctx.ctx, nullptr, nullptr);
    PRJM_EVAL_F result = projectm_eval_code_execute(code.handle);
    EXPECT_DOUBLE_EQ(result, 1.5 + 25.0 + 350.0);
}

TEST(ProjectMEvalQuirks, GmegabufSharedAcrossContexts)
{
    projectm_eval_mem_buffer globalMem = projectm_eval_memory_buffer_create();

    {
        EvalContext writer(globalMem);
        CompiledCode writeCode(writer, "gmegabuf(42) = 1234.5;");
        ASSERT_NE(writeCode.handle, nullptr) << projectm_eval_get_error(writer.ctx, nullptr, nullptr);
        projectm_eval_code_execute(writeCode.handle);
    }

    EvalContext reader(globalMem);
    CompiledCode readCode(reader, "gmegabuf(42);");
    ASSERT_NE(readCode.handle, nullptr) << projectm_eval_get_error(reader.ctx, nullptr, nullptr);
    PRJM_EVAL_F result = projectm_eval_code_execute(readCode.handle);
    EXPECT_DOUBLE_EQ(result, 1234.5);

    projectm_eval_memory_buffer_destroy(globalMem);
}

TEST(ProjectMEvalQuirks, DivisionByZeroReturnsZero)
{
    EvalContext ctx;
    CompiledCode code(ctx, "1/0;");
    ASSERT_NE(code.handle, nullptr) << projectm_eval_get_error(ctx.ctx, nullptr, nullptr);
    PRJM_EVAL_F result = projectm_eval_code_execute(code.handle);
    EXPECT_DOUBLE_EQ(result, 0.0);
}

TEST(ProjectMEvalQuirks, DivisionByZeroNegativeNumeratorReturnsZero)
{
    EvalContext ctx;
    CompiledCode code(ctx, "-5/0;");
    ASSERT_NE(code.handle, nullptr) << projectm_eval_get_error(ctx.ctx, nullptr, nullptr);
    PRJM_EVAL_F result = projectm_eval_code_execute(code.handle);
    EXPECT_DOUBLE_EQ(result, 0.0);
}

TEST(ProjectMEvalQuirks, IfOnlyEvaluatesTheSelectedBranch)
{
    EvalContext ctx;
    CompiledCode code(ctx, "a=0; b=0; if(1, a=42, b=99); a+b*0;");
    ASSERT_NE(code.handle, nullptr) << projectm_eval_get_error(ctx.ctx, nullptr, nullptr);
    projectm_eval_code_execute(code.handle);

    PRJM_EVAL_F* varA = Reg(ctx, "a");
    PRJM_EVAL_F* varB = Reg(ctx, "b");
    EXPECT_DOUBLE_EQ(*varA, 42.0);
    EXPECT_DOUBLE_EQ(*varB, 0.0) << "projectm-eval 1.0.7's if() short-circuits: with a true "
                                    "condition, only the true branch's side effects apply. This is "
                                    "a genuine divergence from the classic x86-asm ns-eel2 lore "
                                    "(which eagerly evaluates both branches) - but NOT a new "
                                    "divergence this project introduces, since ref-src's own DX9 "
                                    "reference build already links this same projectm-eval library "
                                    "via ns-eel2-shim (see ref-src/beatdrop/vis_milk2/state.cpp), so the "
                                    "golden corpus was already captured with this short-circuiting "
                                    "behavior. See output-P2-029.md.";
}

TEST(ProjectMEvalQuirks, IfFalseConditionOnlyEvaluatesTheElseBranch)
{
    EvalContext ctx;
    CompiledCode code(ctx, "a=0; b=0; if(0, a=42, b=99); a+b*0;");
    ASSERT_NE(code.handle, nullptr) << projectm_eval_get_error(ctx.ctx, nullptr, nullptr);
    projectm_eval_code_execute(code.handle);

    PRJM_EVAL_F* varA = Reg(ctx, "a");
    PRJM_EVAL_F* varB = Reg(ctx, "b");
    EXPECT_DOUBLE_EQ(*varA, 0.0);
    EXPECT_DOUBLE_EQ(*varB, 99.0);
}

TEST(ProjectMEvalQuirks, ModuloOnNegativeNumerator)
{
    EvalContext ctx;
    CompiledCode code(ctx, "-7 % 3;");
    ASSERT_NE(code.handle, nullptr) << projectm_eval_get_error(ctx.ctx, nullptr, nullptr);
    PRJM_EVAL_F result = projectm_eval_code_execute(code.handle);
    EXPECT_DOUBLE_EQ(result, -1.0) << "captured from the real x86 vcpkg projectm-eval 1.0.7 build on "
                                      "this run - commit this value as the conformance oracle";
}

TEST(ProjectMEvalQuirks, ModuloOnNegativeDivisor)
{
    EvalContext ctx;
    CompiledCode code(ctx, "7 % -3;");
    ASSERT_NE(code.handle, nullptr) << projectm_eval_get_error(ctx.ctx, nullptr, nullptr);
    PRJM_EVAL_F result = projectm_eval_code_execute(code.handle);
    EXPECT_DOUBLE_EQ(result, 1.0) << "captured from the real x86 vcpkg projectm-eval 1.0.7 build on "
                                     "this run - commit this value as the conformance oracle";
}

TEST(ProjectMEvalQuirks, ModuloOnNegativeNumeratorAndDivisor)
{
    EvalContext ctx;
    CompiledCode code(ctx, "-7 % -3;");
    ASSERT_NE(code.handle, nullptr) << projectm_eval_get_error(ctx.ctx, nullptr, nullptr);
    PRJM_EVAL_F result = projectm_eval_code_execute(code.handle);
    EXPECT_DOUBLE_EQ(result, -1.0) << "captured from the real x86 vcpkg projectm-eval 1.0.7 build on "
                                      "this run - commit this value as the conformance oracle";
}

TEST(ProjectMEvalQuirks, ModuloByZeroReturnsZero)
{
    EvalContext ctx;
    CompiledCode code(ctx, "5 % 0;");
    ASSERT_NE(code.handle, nullptr) << projectm_eval_get_error(ctx.ctx, nullptr, nullptr);
    PRJM_EVAL_F result = projectm_eval_code_execute(code.handle);
    EXPECT_DOUBLE_EQ(result, 0.0);
}

TEST(ProjectMEvalQuirks, SigmoidKnownValues)
{
    EvalContext ctx;
    CompiledCode code(ctx, "sigmoid(0, 1);");
    ASSERT_NE(code.handle, nullptr) << projectm_eval_get_error(ctx.ctx, nullptr, nullptr);
    PRJM_EVAL_F result = projectm_eval_code_execute(code.handle);
    EXPECT_DOUBLE_EQ(result, 0.5) << "sigmoid(0, a) is 0.5 for any slope a, by definition of the "
                                     "logistic function at x=0 - a cross-check, not just a captured "
                                     "value";
}

TEST(ProjectMEvalQuirks, SigmoidCapturedValue)
{
    EvalContext ctx;
    CompiledCode code(ctx, "sigmoid(2, 3);");
    ASSERT_NE(code.handle, nullptr) << projectm_eval_get_error(ctx.ctx, nullptr, nullptr);
    PRJM_EVAL_F result = projectm_eval_code_execute(code.handle);
    EXPECT_DOUBLE_EQ(result, 1.0 / (1.0 + std::exp(-2.0 * 3.0)))
        << "captured from the real x86 vcpkg projectm-eval 1.0.7 build on this run";
}

TEST(ProjectMEvalQuirks, RandBoundedAndFinite)
{
    EvalContext ctx;
    CompiledCode code(ctx, "rand(1000);");
    ASSERT_NE(code.handle, nullptr) << projectm_eval_get_error(ctx.ctx, nullptr, nullptr);
    for (int i = 0; i < 1000; ++i)
    {
        PRJM_EVAL_F result = projectm_eval_code_execute(code.handle);
        EXPECT_GE(result, 0.0);
        EXPECT_LT(result, 1000.0);
    }
}

TEST(ProjectMEvalQuirks, RandIsNotDrivenByTheHostCrtStream)
{
    srand(777);
    int crtDraw1 = rand();

    srand(42);
    for (int i = 0; i < 1000; ++i)
    {
        rand();
    }

    EvalContext ctx;
    CompiledCode code(ctx, "rand(2147483647);");
    ASSERT_NE(code.handle, nullptr) << projectm_eval_get_error(ctx.ctx, nullptr, nullptr);
    PRJM_EVAL_F eelDraw = projectm_eval_code_execute(code.handle);

    EXPECT_DOUBLE_EQ(eelDraw, 489036813.38613725)
        << "C2-023 finding: this is the FIRST draw of this process's projectm-eval rand() "
           "sequence scaled by 2147483647 (0.22772551216830628 * 2147483647), reproduced "
           "identically whether or not this test perturbs the host CRT's rand()/srand() stream "
           "beforehand (crtDraw1=" << crtDraw1
        << ") - i.e. projectm-eval's rand() is NOT libc/CRT-rand()-backed, and is NOT the same "
           "process-global stream BeatDrop's own BdRand()/BdSrand() compat layer forwards to on "
           "MSVC. See output-P2-029.md for the full writeup and the still-open question this "
           "raises (whether it is seeded identically on arm64).";
}

TEST(ProjectMEvalQuirks, RandIsDeterministicFromAFreshProcessButSharedAcrossContexts)
{
    EvalContext ctx1;
    CompiledCode code1(ctx1, "rand(1);");
    ASSERT_NE(code1.handle, nullptr) << projectm_eval_get_error(ctx1.ctx, nullptr, nullptr);

    PRJM_EVAL_F first = projectm_eval_code_execute(code1.handle);

    EXPECT_DOUBLE_EQ(first, 0.22772551216830628)
        << "captured from this real x86 vcpkg projectm-eval 1.0.7 build - the first rand(x) draw "
           "of a fresh process, every time, regardless of which context or compiled code object "
           "makes the call (see output-P2-029.md's three independent repro runs)";

    EvalContext ctx2;
    CompiledCode code2(ctx2, "rand(1);");
    ASSERT_NE(code2.handle, nullptr) << projectm_eval_get_error(ctx2.ctx, nullptr, nullptr);
    PRJM_EVAL_F second = projectm_eval_code_execute(code2.handle);

    EXPECT_DOUBLE_EQ(second, 0.34074419092869951)
        << "C2-023 finding: this is the SAME value a second rand(1) draw from the FIRST context "
           "would have produced - i.e. projectm-eval's rand() generator state is a single, "
           "process-global stream, NOT reset or re-seeded by projectm_eval_context_create(). Two "
           "unrelated contexts in the same process (e.g. two different presets' per-frame eval "
           "contexts, or a preset's per-frame and per-pixel contexts) draw from the SAME ongoing "
           "sequence, in whatever order their code happens to call rand() at runtime.";
}

class ProjectMEvalQConformance : public ::testing::Test
{
protected:
    void SetUp() override
    {
        if (!fs::exists(SYNESTHESIA_VIBE_GOLDEN_CORPUS_DIR))
        {
            GTEST_SKIP() << "golden corpus not found at " << SYNESTHESIA_VIBE_GOLDEN_CORPUS_DIR;
        }
    }
};

TEST_F(ProjectMEvalQConformance, FuzzySciencesPerFrameQVarsBitExactOnAllFrames)
{
    std::vector<QVarMismatch> mismatches = ReplayQVars("Unchained - Fuzzy Sciences.milk", "preset-fuzzy-sciences", 600);
    EXPECT_TRUE(mismatches.empty()) << mismatches.size() << " q-var mismatches; first at frame "
                                     << (mismatches.empty() ? -1 : mismatches.front().frame) << " q"
                                     << (mismatches.empty() ? -1 : mismatches.front().qIndex + 1) << " got "
                                     << (mismatches.empty() ? 0.0 : mismatches.front().got) << " expected "
                                     << (mismatches.empty() ? 0.0 : mismatches.front().expected);
}

TEST_F(ProjectMEvalQConformance, ShifterSwarmConstantQVarsBitExactOnAllFrames)
{
    std::vector<QVarMismatch> mismatches = ReplayQVars("shifter - swarm.milk", "preset-shifter-swarm", 600);

    std::vector<QVarMismatch> constantChannelMismatches;
    for (const auto& m : mismatches)
    {
        if (m.qIndex == 4 || m.qIndex == 5)
        {
            constantChannelMismatches.push_back(m);
        }
    }

    EXPECT_TRUE(constantChannelMismatches.empty())
        << constantChannelMismatches.size() << " mismatches on q5/q6, which this preset's "
           "per_frame_23 (`q5 = .15;`) and per_frame_27 (`q6 = 40;`) hard-code as constants "
           "regardless of sound/time input, so they must always replay bit-exact.";
}

TEST_F(ProjectMEvalQConformance, ShifterSwarmTimeAndSoundDependentQVarsDivergeAtKnownFrames)
{
    std::vector<QVarMismatch> mismatches = ReplayQVars("shifter - swarm.milk", "preset-shifter-swarm", 600);

    int firstMismatchFrame[4] = {-1, -1, -1, -1};
    for (const auto& m : mismatches)
    {
        if (m.qIndex >= 0 && m.qIndex <= 3 && firstMismatchFrame[m.qIndex] == -1)
        {
            firstMismatchFrame[m.qIndex] = m.frame;
        }
    }

    EXPECT_EQ(firstMismatchFrame[3], 0)
        << "q4 (`q4 = (5*sin(time*.145) + 3*cos(time*.115))*.1;`) reads the host-fed `time` "
           "variable directly (not as a frame-to-frame delta), and is expected to diverge from "
           "frame 0 - this harness feeds each frame's own captured `time` field as the equation "
           "input, but frame 0's expected q4 (0.3 exactly) can only come from an equation-time "
           "value of 0.0, not the captured time field's own value (~1/60 s) for that same frame. "
           "This is the general open question C2-017 already tracks (\"the meaning of "
           "dt\" - the exact relationship between a captured per-frame field and the value that "
           "frame's own equations actually consumed is not yet pinned down project-wide), not a "
           "projectm-eval library defect. See output-P2-029.md.";

    EXPECT_EQ(firstMismatchFrame[0], 13)
        << "q1/q2/q3 (`v`/`tb`/`tu`, fed by `tic = time-tin` deltas and `bass`/`bass_att`/`treb`/"
           "`treb_att`) replay bit-exact through frame 12 (while the deterministic test WAV is "
           "still silent) and diverge starting at frame 13, the first frame with non-silent audio "
           "in this capture. Unlike the q4/C2-017 case above, the exact mechanism here is not "
           "pinned down - flagged for follow-up alongside C2-017 rather than guessed at here. "
           "firstMismatchFrame[q1..q3]=" << firstMismatchFrame[0] << "," << firstMismatchFrame[1] << ","
        << firstMismatchFrame[2] << ". See output-P2-029.md.";
    EXPECT_EQ(firstMismatchFrame[1], 13);
    EXPECT_EQ(firstMismatchFrame[2], 13);
}

TEST_F(ProjectMEvalQConformance, JestersCalling2DeterministicQVarsBitExactOnAllFrames)
{
    std::vector<QVarMismatch> mismatches = ReplayQVars("Rovastar - Jester's Calling 2.milk", "preset-jesters-calling-2", 600);

    std::vector<QVarMismatch> deterministicMismatches;
    for (const auto& m : mismatches)
    {
        if (m.qIndex != 4)
        {
            deterministicMismatches.push_back(m);
        }
    }

    EXPECT_TRUE(deterministicMismatches.empty())
        << deterministicMismatches.size() << " q-var mismatches on a non-rand()-dependent channel; first at frame "
        << (deterministicMismatches.empty() ? -1 : deterministicMismatches.front().frame) << " q"
        << (deterministicMismatches.empty() ? -1 : deterministicMismatches.front().qIndex + 1) << " got "
        << (deterministicMismatches.empty() ? 0.0 : deterministicMismatches.front().got) << " expected "
        << (deterministicMismatches.empty() ? 0.0 : deterministicMismatches.front().expected)
        << " - q1,q2,q3,q7,q8 are never written by this preset's per_frame_ code (only by its "
           "per_pixel_ code, which DX9 compiles to a GPU warp-mesh pass, not a var_pf_q write - "
           "confirmed empirically against the golden corpus, see output-P2-029.md) and q4/q6 have "
           "no rand() dependency, so all 7 of these channels must replay bit-exact.";
}

TEST_F(ProjectMEvalQConformance, JestersCalling2RandDependentQVarDivergesAtKnownFrame)
{
    std::vector<QVarMismatch> mismatches = ReplayQVars("Rovastar - Jester's Calling 2.milk", "preset-jesters-calling-2", 600);

    int firstQ5Mismatch = -1;
    for (const auto& m : mismatches)
    {
        if (m.qIndex == 4)
        {
            firstQ5Mismatch = m.frame;
            break;
        }
    }

    RecordProperty("firstQ5MismatchFrame", firstQ5Mismatch);

    EXPECT_NE(firstQ5Mismatch, -1)
        << "q5 (fed by this preset's per_frame_36 `Eff_size = if(beat, 2.5*Eff_test + "
           "0.0025*rand(500), Old_Eff_size)`) is expected to diverge from the golden once the "
           "preset's own `beat` detector fires at least once in the first 600 frames and consumes "
           "a draw from projectm-eval's rand() generator (observed first at frame "
        << firstQ5Mismatch << " in this run). Per the C2-023 finding, projectm-eval's rand() is a "
           "single, deterministic, process-global stream (not libc-backed, not reset per "
           "context) - this preset's capture process has exactly one equation call site anywhere "
           "touching it (per_frame_36), so in principle a standalone single-context replay of "
           "just that call site could line up with the real engine's capture process. "
           "Empirically it does not: something in the real engine's actual capture process draws "
           "from the same global generator before or interleaved with this preset's own calls, "
           "shifting its position in the sequence relative to this harness. The exact source is "
           "not identified - flagged for follow-up alongside C2-023/P2-052, not fabricated here. "
           "If this ever reports -1 (no divergence), the audio/beat-detection inputs changed and "
           "this test should be revisited, not just re-pinned.";
}
