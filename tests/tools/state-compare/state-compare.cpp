//
//  state-compare.cpp
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-07.
//

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <iterator>
#include <stdio.h>
#include <string>
#include <utility>
#include <vector>

#if defined(_WIN32)
#define SYNESTHESIA_VIBE_POPEN _popen
#define SYNESTHESIA_VIBE_PCLOSE _pclose
#else
#define SYNESTHESIA_VIBE_POPEN popen
#define SYNESTHESIA_VIBE_PCLOSE pclose
#endif

namespace {

namespace fs = std::filesystem;

constexpr double kAbsTolerance = 1e-5;
constexpr double kRelTolerance = 1e-5;
constexpr double kRelativeGateFloor = 1.0;
constexpr double kDefaultMeanSsim = 0.98;
constexpr double kDefaultMinSsim = 0.95;
constexpr int kSsimAbsDiffTolerance = 255;
constexpr std::size_t kFloatHexDigits = 8;
constexpr std::size_t kDoubleHexDigits = 16;
constexpr std::size_t kQValueCount = 32;
constexpr std::size_t kSsimChannelsPerPair = 3;
constexpr const char *kAlwaysExcludedField = "passes";
constexpr const char *kFramePrefix = "frame_";
constexpr const char *kJsonSuffix = ".json";
constexpr const char *kPngSuffix = ".png";
constexpr const char *kCompositeSuffix = "-composite.png";
constexpr int kExitPass = 0;
constexpr int kExitDiverge = 1;
constexpr int kExitError = 2;

enum class ValueKind { Null, Bool, Number, String, Object, Array };

const char *KindName(ValueKind kind) {
  switch (kind) {
    case ValueKind::Null: return "null";
    case ValueKind::Bool: return "bool";
    case ValueKind::Number: return "number";
    case ValueKind::String: return "string";
    case ValueKind::Object: return "object";
    case ValueKind::Array: return "array";
  }
  return "unknown";
}

int HexDigit(char value) {
  if (value >= '0' && value <= '9') {
    return value - '0';
  }
  if (value >= 'a' && value <= 'f') {
    return value - 'a' + 10;
  }
  if (value >= 'A' && value <= 'F') {
    return value - 'A' + 10;
  }
  return -1;
}

void AppendUtf8(std::string &out, unsigned int codePoint) {
  if (codePoint <= 0x7Fu) {
    out.push_back(static_cast<char>(codePoint));
  } else if (codePoint <= 0x7FFu) {
    out.push_back(static_cast<char>(0xC0u | (codePoint >> 6)));
    out.push_back(static_cast<char>(0x80u | (codePoint & 0x3Fu)));
  } else {
    out.push_back(static_cast<char>(0xE0u | (codePoint >> 12)));
    out.push_back(static_cast<char>(0x80u | ((codePoint >> 6) & 0x3Fu)));
    out.push_back(static_cast<char>(0x80u | (codePoint & 0x3Fu)));
  }
}

struct JsonValue {
  ValueKind kind = ValueKind::Null;
  bool boolean = false;
  std::string number;
  std::string text;
  std::vector<std::pair<std::string, int>> members;
  std::vector<int> elements;
};

class JsonParser {
 public:
  explicit JsonParser(const std::string &text) : text_(text), pos_(0), root_(-1) {}

  bool Parse() {
    SkipWhitespace();
    if (!ParseValue(root_)) {
      return false;
    }
    SkipWhitespace();
    if (pos_ != text_.size()) {
      return Fail("trailing content after the top-level value");
    }
    return true;
  }

  bool Ok() const { return error_.empty(); }
  const std::string &Error() const { return error_; }
  int Root() const { return root_; }

  const JsonValue &At(int index) const { return nodes_[static_cast<std::size_t>(index)]; }

  int FindMember(int objectIndex, const std::string &key) const {
    const JsonValue &object = At(objectIndex);
    for (const std::pair<std::string, int> &member : object.members) {
      if (member.first == key) {
        return member.second;
      }
    }
    return -1;
  }

 private:
  bool Fail(const std::string &message) {
    if (error_.empty()) {
      error_ = message + " at byte " + std::to_string(pos_);
    }
    return false;
  }

  int NewNode(ValueKind kind) {
    JsonValue node;
    node.kind = kind;
    nodes_.push_back(std::move(node));
    return static_cast<int>(nodes_.size()) - 1;
  }

  void SkipWhitespace() {
    while (pos_ < text_.size()) {
      const char value = text_[pos_];
      if (value == ' ' || value == '\t' || value == '\n' || value == '\r') {
        ++pos_;
        continue;
      }
      break;
    }
  }

  bool ParseValue(int &index) {
    SkipWhitespace();
    if (pos_ >= text_.size()) {
      return Fail("unexpected end of input");
    }
    const char value = text_[pos_];
    if (value == '{') {
      return ParseObject(index);
    }
    if (value == '[') {
      return ParseArray(index);
    }
    if (value == '"') {
      return ParseStringValue(index);
    }
    if (value == 't' || value == 'f') {
      return ParseBool(index);
    }
    if (value == 'n') {
      return ParseNull(index);
    }
    return ParseNumber(index);
  }

  bool ParseStringBody(std::string &out) {
    if (pos_ >= text_.size() || text_[pos_] != '"') {
      return Fail("expected a string");
    }
    ++pos_;
    out.clear();
    while (true) {
      if (pos_ >= text_.size()) {
        return Fail("unterminated string");
      }
      const char value = text_[pos_++];
      if (value == '"') {
        return true;
      }
      if (value != '\\') {
        out.push_back(value);
        continue;
      }
      if (pos_ >= text_.size()) {
        return Fail("unterminated escape");
      }
      const char escape = text_[pos_++];
      switch (escape) {
        case '"': out.push_back('"'); break;
        case '\\': out.push_back('\\'); break;
        case '/': out.push_back('/'); break;
        case 'b': out.push_back('\b'); break;
        case 'f': out.push_back('\f'); break;
        case 'n': out.push_back('\n'); break;
        case 'r': out.push_back('\r'); break;
        case 't': out.push_back('\t'); break;
        case 'u': {
          if (pos_ + 4 > text_.size()) {
            return Fail("truncated \\u escape");
          }
          unsigned int codePoint = 0;
          for (int i = 0; i < 4; ++i) {
            const int digit = HexDigit(text_[pos_ + static_cast<std::size_t>(i)]);
            if (digit < 0) {
              return Fail("malformed \\u escape");
            }
            codePoint = codePoint * 16u + static_cast<unsigned int>(digit);
          }
          pos_ += 4;
          AppendUtf8(out, codePoint);
          break;
        }
        default:
          return Fail("unknown escape sequence");
      }
    }
  }

  bool ParseStringValue(int &index) {
    std::string text;
    if (!ParseStringBody(text)) {
      return false;
    }
    index = NewNode(ValueKind::String);
    nodes_[static_cast<std::size_t>(index)].text = std::move(text);
    return true;
  }

  bool ParseLiteral(const char *literal, ValueKind kind, bool booleanValue, int &index) {
    const std::size_t length = std::strlen(literal);
    if (pos_ + length > text_.size() || text_.compare(pos_, length, literal) != 0) {
      return Fail(std::string("expected '") + literal + "'");
    }
    pos_ += length;
    index = NewNode(kind);
    nodes_[static_cast<std::size_t>(index)].boolean = booleanValue;
    return true;
  }

  bool ParseBool(int &index) {
    if (text_[pos_] == 't') {
      return ParseLiteral("true", ValueKind::Bool, true, index);
    }
    return ParseLiteral("false", ValueKind::Bool, false, index);
  }

  bool ParseNull(int &index) { return ParseLiteral("null", ValueKind::Null, false, index); }

  bool ParseNumber(int &index) {
    const std::size_t start = pos_;
    while (pos_ < text_.size()) {
      const char value = text_[pos_];
      const bool isNumberChar = (value >= '0' && value <= '9') || value == '-' || value == '+' ||
                                value == '.' || value == 'e' || value == 'E';
      if (!isNumberChar) {
        break;
      }
      ++pos_;
    }
    if (pos_ == start) {
      return Fail("expected a value");
    }
    index = NewNode(ValueKind::Number);
    nodes_[static_cast<std::size_t>(index)].number = text_.substr(start, pos_ - start);
    return true;
  }

  bool ParseObject(int &index) {
    index = NewNode(ValueKind::Object);
    ++pos_;
    SkipWhitespace();
    if (pos_ < text_.size() && text_[pos_] == '}') {
      ++pos_;
      return true;
    }
    while (true) {
      SkipWhitespace();
      std::string key;
      if (!ParseStringBody(key)) {
        return false;
      }
      SkipWhitespace();
      if (pos_ >= text_.size() || text_[pos_] != ':') {
        return Fail("expected ':' after an object key");
      }
      ++pos_;
      int child = -1;
      if (!ParseValue(child)) {
        return false;
      }
      nodes_[static_cast<std::size_t>(index)].members.emplace_back(std::move(key), child);
      SkipWhitespace();
      if (pos_ >= text_.size()) {
        return Fail("unterminated object");
      }
      if (text_[pos_] == ',') {
        ++pos_;
        continue;
      }
      if (text_[pos_] == '}') {
        ++pos_;
        return true;
      }
      return Fail("expected ',' or '}' in an object");
    }
  }

  bool ParseArray(int &index) {
    index = NewNode(ValueKind::Array);
    ++pos_;
    SkipWhitespace();
    if (pos_ < text_.size() && text_[pos_] == ']') {
      ++pos_;
      return true;
    }
    while (true) {
      int child = -1;
      if (!ParseValue(child)) {
        return false;
      }
      nodes_[static_cast<std::size_t>(index)].elements.push_back(child);
      SkipWhitespace();
      if (pos_ >= text_.size()) {
        return Fail("unterminated array");
      }
      if (text_[pos_] == ',') {
        ++pos_;
        continue;
      }
      if (text_[pos_] == ']') {
        ++pos_;
        return true;
      }
      return Fail("expected ',' or ']' in an array");
    }
  }

  const std::string &text_;
  std::size_t pos_;
  std::vector<JsonValue> nodes_;
  std::string error_;
  int root_;
};

bool ReadTextFile(const fs::path &path, std::string &out, std::string &error) {
  std::FILE *file = std::fopen(path.string().c_str(), "rb");
  if (file == nullptr) {
    error = "cannot open '" + path.string() + "'";
    return false;
  }
  std::string contents;
  char buffer[65536];
  std::size_t read = 0;
  while ((read = std::fread(buffer, 1, sizeof(buffer), file)) > 0) {
    contents.append(buffer, read);
  }
  const bool failed = std::ferror(file) != 0;
  std::fclose(file);
  if (failed) {
    error = "read error on '" + path.string() + "'";
    return false;
  }
  out = std::move(contents);
  return true;
}

bool WriteTextFile(const fs::path &path, const std::string &text, std::string &error) {
  std::error_code code;
  const fs::path parent = path.parent_path();
  if (!parent.empty()) {
    fs::create_directories(parent, code);
  }
  std::FILE *file = std::fopen(path.string().c_str(), "wb");
  if (file == nullptr) {
    error = "cannot write '" + path.string() + "'";
    return false;
  }
  const std::size_t written = text.empty() ? 0 : std::fwrite(text.data(), 1, text.size(), file);
  std::fclose(file);
  if (written != text.size()) {
    error = "short write on '" + path.string() + "'";
    return false;
  }
  return true;
}

bool IsHexLeaf(const JsonValue &value) {
  if (value.kind != ValueKind::Object || value.members.size() != 2) {
    return false;
  }
  bool hasHex = false;
  bool hasDec = false;
  for (const std::pair<std::string, int> &member : value.members) {
    if (member.first == "hex") {
      hasHex = true;
    } else if (member.first == "dec") {
      hasDec = true;
    } else {
      return false;
    }
  }
  return hasHex && hasDec;
}

struct HexBits {
  bool ok = false;
  bool isDouble = false;
  std::uint64_t bits = 0;
  std::string text;
};

HexBits DecodeHexLeaf(const JsonParser &parser, int index) {
  HexBits result;
  const JsonValue &value = parser.At(index);
  if (!IsHexLeaf(value)) {
    return result;
  }
  for (const std::pair<std::string, int> &member : value.members) {
    if (member.first != "hex") {
      continue;
    }
    const JsonValue &hexValue = parser.At(member.second);
    if (hexValue.kind != ValueKind::String) {
      return result;
    }
    result.text = hexValue.text;
    std::string digits = hexValue.text;
    if (digits.size() > 2 && digits[0] == '0' && (digits[1] == 'x' || digits[1] == 'X')) {
      digits = digits.substr(2);
    }
    if (digits.size() != kFloatHexDigits && digits.size() != kDoubleHexDigits) {
      return result;
    }
    std::uint64_t bits = 0;
    for (char digitChar : digits) {
      const int digit = HexDigit(digitChar);
      if (digit < 0) {
        return result;
      }
      bits = (bits << 4) | static_cast<std::uint64_t>(digit);
    }
    result.bits = bits;
    result.isDouble = digits.size() == kDoubleHexDigits;
    result.ok = true;
    return result;
  }
  return result;
}

double BitsToDouble(std::uint64_t bits, bool isDouble) {
  if (isDouble) {
    double value = 0.0;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
  }
  const std::uint32_t narrow = static_cast<std::uint32_t>(bits & 0xFFFFFFFFu);
  float value = 0.0f;
  std::memcpy(&value, &narrow, sizeof(value));
  return static_cast<double>(value);
}

std::string FormatHexBits(std::uint64_t bits, bool isDouble) {
  char buffer[32];
  if (isDouble) {
    std::snprintf(buffer, sizeof(buffer), "0x%016llX",
                  static_cast<unsigned long long>(bits));
  } else {
    std::snprintf(buffer, sizeof(buffer), "0x%08X",
                  static_cast<unsigned int>(bits & 0xFFFFFFFFu));
  }
  return buffer;
}

std::string FormatNumber(double value) {
  char buffer[64];
  std::snprintf(buffer, sizeof(buffer), "%.9g", value);
  return buffer;
}

struct FieldDivergence {
  bool present = false;
  bool shape = false;
  bool nonFinite = false;
  std::string path;
  std::string detail;
};

struct CompareContext {
  bool exactMode = false;
  std::vector<std::string> excluded;
  FieldDivergence divergence;
};

bool IsExcluded(const CompareContext &context, const std::string &key) {
  return std::find(context.excluded.begin(), context.excluded.end(), key) != context.excluded.end();
}

bool RecordDivergence(CompareContext &context, const std::string &path, bool shape, bool nonFinite,
                      const std::string &detail) {
  if (!context.divergence.present) {
    context.divergence.present = true;
    context.divergence.shape = shape;
    context.divergence.nonFinite = nonFinite;
    context.divergence.path = path;
    context.divergence.detail = detail;
  }
  return false;
}

std::string JoinMember(const std::string &path, const std::string &key) {
  return path.empty() ? key : path + "." + key;
}

std::string JoinIndex(const std::string &path, std::size_t index) {
  return path + "[" + std::to_string(index) + "]";
}

bool CompareHexLeaf(const JsonParser &golden, int goldenIndex, const JsonParser &actual,
                    int actualIndex, const std::string &path, CompareContext &context) {
  const HexBits goldenBits = DecodeHexLeaf(golden, goldenIndex);
  const HexBits actualBits = DecodeHexLeaf(actual, actualIndex);
  if (!goldenBits.ok || !actualBits.ok) {
    return RecordDivergence(context, path, true, false,
                            "undecodable float leaf (golden='" + goldenBits.text +
                                "' actual='" + actualBits.text + "')");
  }
  if (goldenBits.isDouble != actualBits.isDouble) {
    return RecordDivergence(context, path, true, false,
                            "hex width mismatch (golden='" + goldenBits.text + "' actual='" +
                                actualBits.text + "')");
  }
  if (goldenBits.bits == actualBits.bits) {
    return true;
  }
  const std::string detail =
      "golden=" + goldenBits.text + " actual=" + actualBits.text;
  if (context.exactMode) {
    return RecordDivergence(context, path, false, false,
                            detail + " (mode=exact: bit-identity required)");
  }
  const double goldenValue = BitsToDouble(goldenBits.bits, goldenBits.isDouble);
  const double actualValue = BitsToDouble(actualBits.bits, actualBits.isDouble);
  const bool goldenFinite = std::isfinite(goldenValue);
  const bool actualFinite = std::isfinite(actualValue);
  if (!goldenFinite || !actualFinite) {
    return RecordDivergence(context, path, false, true,
                            detail + " (" + FormatNumber(goldenValue) + " vs " +
                                FormatNumber(actualValue) +
                                "; non-finite values require bit-identity)");
  }
  const double absDiff = std::abs(goldenValue - actualValue);
  const bool relativeGate = std::abs(goldenValue) > kRelativeGateFloor;
  const double relativeDiff =
      std::abs(goldenValue) > 0.0 ? absDiff / std::abs(goldenValue) : 0.0;
  const bool relativeOk = relativeGate && relativeDiff <= kRelTolerance;
  if (absDiff <= kAbsTolerance || relativeOk) {
    return true;
  }
  char numbers[192];
  std::snprintf(numbers, sizeof(numbers),
                " (%s vs %s, abs-diff=%.9g rel-diff=%.9g rel-gate=%s, |golden|=%.9g)",
                FormatNumber(goldenValue).c_str(), FormatNumber(actualValue).c_str(), absDiff,
                relativeDiff, relativeGate ? "on" : "off", std::abs(goldenValue));
  return RecordDivergence(context, path, false, false,
                          detail + numbers +
                              " exceeds tolerance (abs<=1e-5, or rel<=1e-5 when |golden|>1)");
}

bool CompareValues(const JsonParser &golden, int goldenIndex, const JsonParser &actual,
                   int actualIndex, const std::string &path, bool isRoot,
                   CompareContext &context) {
  const JsonValue &goldenValue = golden.At(goldenIndex);
  const JsonValue &actualValue = actual.At(actualIndex);

  if (goldenValue.kind != actualValue.kind) {
    return RecordDivergence(context, path, true, false,
                            std::string("kind mismatch (golden=") + KindName(goldenValue.kind) +
                                " actual=" + KindName(actualValue.kind) + ")");
  }

  switch (goldenValue.kind) {
    case ValueKind::Null:
      return true;
    case ValueKind::Bool:
      if (goldenValue.boolean == actualValue.boolean) {
        return true;
      }
      return RecordDivergence(context, path, false, false,
                              std::string("golden=") + (goldenValue.boolean ? "true" : "false") +
                                  " actual=" + (actualValue.boolean ? "true" : "false"));
    case ValueKind::Number:
      if (goldenValue.number == actualValue.number) {
        return true;
      }
      return RecordDivergence(context, path, false, false,
                              "golden=" + goldenValue.number + " actual=" + actualValue.number);
    case ValueKind::String:
      if (goldenValue.text == actualValue.text) {
        return true;
      }
      return RecordDivergence(context, path, false, false,
                              "golden='" + goldenValue.text + "' actual='" + actualValue.text + "'");
    case ValueKind::Object: {
      if (IsHexLeaf(goldenValue) || IsHexLeaf(actualValue)) {
        if (IsHexLeaf(goldenValue) && IsHexLeaf(actualValue)) {
          return CompareHexLeaf(golden, goldenIndex, actual, actualIndex, path, context);
        }
        return RecordDivergence(context, path, true, false, "float leaf on one side only");
      }
      for (const std::pair<std::string, int> &member : goldenValue.members) {
        if (isRoot && IsExcluded(context, member.first)) {
          continue;
        }
        const std::string childPath = JoinMember(path, member.first);
        const int actualChild = actual.FindMember(actualIndex, member.first);
        if (actualChild < 0) {
          RecordDivergence(context, childPath, true, false, "key missing on the actual side");
          return false;
        }
        if (!CompareValues(golden, member.second, actual, actualChild, childPath, false, context)) {
          return false;
        }
      }
      for (const std::pair<std::string, int> &member : actualValue.members) {
        if (isRoot && IsExcluded(context, member.first)) {
          continue;
        }
        if (golden.FindMember(goldenIndex, member.first) < 0) {
          return RecordDivergence(context, JoinMember(path, member.first), true, false,
                                  "unexpected key on the actual side");
        }
      }
      return true;
    }
    case ValueKind::Array: {
      if (goldenValue.elements.size() != actualValue.elements.size()) {
        return RecordDivergence(context, path, true, false,
                                "length mismatch (golden=" +
                                    std::to_string(goldenValue.elements.size()) + " actual=" +
                                    std::to_string(actualValue.elements.size()) + ")");
      }
      for (std::size_t index = 0; index < goldenValue.elements.size(); ++index) {
        if (!CompareValues(golden, goldenValue.elements[index], actual,
                           actualValue.elements[index], JoinIndex(path, index), false, context)) {
          return false;
        }
      }
      return true;
    }
  }
  return true;
}

bool ReadPlainInteger(const JsonParser &parser, int index, long long &out) {
  const JsonValue &value = parser.At(index);
  if (value.kind != ValueKind::Number || value.number.empty()) {
    return false;
  }
  char *end = nullptr;
  const long long parsed = std::strtoll(value.number.c_str(), &end, 10);
  if (end == nullptr || *end != '\0') {
    return false;
  }
  out = parsed;
  return true;
}

bool CheckGoldenInvariants(const JsonParser &golden, CompareContext &context) {
  const int root = golden.Root();
  const int qIndex = golden.FindMember(root, "q");
  if (qIndex >= 0) {
    const JsonValue &q = golden.At(qIndex);
    if (q.kind == ValueKind::Array && q.elements.size() != kQValueCount) {
      return RecordDivergence(context, "q", true, false,
                              "golden q length " + std::to_string(q.elements.size()) +
                                  ", expected 32");
    }
  }
  const int meshIndex = golden.FindMember(root, "mesh");
  if (meshIndex < 0) {
    return true;
  }
  const JsonValue &mesh = golden.At(meshIndex);
  if (mesh.kind != ValueKind::Object) {
    return true;
  }
  const int vertsIndex = golden.FindMember(meshIndex, "verts");
  const int gridXIndex = golden.FindMember(meshIndex, "grid_x");
  const int gridYIndex = golden.FindMember(meshIndex, "grid_y");
  if (vertsIndex < 0 || gridXIndex < 0 || gridYIndex < 0) {
    return true;
  }
  long long gridX = 0;
  long long gridY = 0;
  const JsonValue &verts = golden.At(vertsIndex);
  if (!ReadPlainInteger(golden, gridXIndex, gridX) || !ReadPlainInteger(golden, gridYIndex, gridY)) {
    return true;
  }
  if (gridX < 0 || gridY < 0 || verts.kind != ValueKind::Array) {
    return true;
  }
  const std::size_t expected =
      static_cast<std::size_t>(gridX + 1) * static_cast<std::size_t>(gridY + 1);
  if (verts.elements.size() != expected) {
    return RecordDivergence(context, "mesh.verts", true, false,
                            "golden mesh.verts length " + std::to_string(verts.elements.size()) +
                                ", expected (grid_x+1)*(grid_y+1)=" + std::to_string(expected));
  }
  return true;
}

struct FramePair {
  int number = 0;
  fs::path golden;
  fs::path actual;
};

bool IsFrameFile(const std::string &name, const char *suffix, int &number) {
  const std::size_t prefixLength = std::strlen(kFramePrefix);
  const std::size_t suffixLength = std::strlen(suffix);
  if (name.size() <= prefixLength + suffixLength) {
    return false;
  }
  if (name.compare(0, prefixLength, kFramePrefix) != 0) {
    return false;
  }
  if (name.compare(name.size() - suffixLength, suffixLength, suffix) != 0) {
    return false;
  }
  const std::string digits = name.substr(prefixLength, name.size() - prefixLength - suffixLength);
  if (digits.empty()) {
    return false;
  }
  int value = 0;
  for (char digitChar : digits) {
    if (digitChar < '0' || digitChar > '9') {
      return false;
    }
    value = value * 10 + (digitChar - '0');
  }
  number = value;
  return true;
}

std::vector<int> ListFrameNumbers(const fs::path &dir, const char *suffix = kJsonSuffix) {
  std::vector<int> numbers;
  std::error_code code;
  fs::directory_iterator iterator(dir, code);
  if (code) {
    return numbers;
  }
  for (const fs::directory_entry &entry : iterator) {
    std::error_code entryCode;
    if (!entry.is_regular_file(entryCode) || entryCode) {
      continue;
    }
    int number = 0;
    if (IsFrameFile(entry.path().filename().string(), suffix, number)) {
      numbers.push_back(number);
    }
  }
  std::sort(numbers.begin(), numbers.end());
  return numbers;
}

std::vector<std::string> ListCaptureIds(const fs::path &dir) {
  std::vector<std::string> ids;
  std::error_code code;
  fs::directory_iterator iterator(dir, code);
  if (code) {
    return ids;
  }
  for (const fs::directory_entry &entry : iterator) {
    std::error_code entryCode;
    if (!entry.is_directory(entryCode) || entryCode) {
      continue;
    }
    if (ListFrameNumbers(entry.path()).empty()) {
      continue;
    }
    ids.push_back(entry.path().filename().string());
  }
  std::sort(ids.begin(), ids.end());
  return ids;
}

std::string FrameFileName(int number, const char *suffix) {
  char buffer[32];
  std::snprintf(buffer, sizeof(buffer), "%s%04d%s", kFramePrefix, number, suffix);
  return buffer;
}

struct CaptureSsim {
  std::size_t samples = 0;
  double meanSum = 0.0;
  double minValue = 1.0;
  std::size_t pairs = 0;
};

struct FrameSsim {
  int number = 0;
  double meanSsim = 0.0;
  double minSsim = 0.0;
  std::string composite;
  std::string error;
};

struct VisualCapture {
  std::string id;
  std::vector<FrameSsim> frames;
  std::size_t errors = 0;
  bool hasSsim = false;
  double meanSum = 0.0;
  double minValue = 1.0;
};

struct VisualRun {
  bool fatal = false;
  std::string fatalError;
  std::vector<VisualCapture> captures;
};

struct CaptureResult {
  std::string id;
  int framesCompared = 0;
  bool numericPass = true;
  bool divergenceFound = false;
  bool shapeDivergence = false;
  bool nonFiniteDivergence = false;
  int divergenceFrame = -1;
  std::string divergencePath;
  std::string divergenceDetail;
  std::size_t unpairedFrames = 0;
  CaptureSsim ssim;
  bool ssimComputed = false;
  bool ssimPass = true;

  bool OverallPass() const { return numericPass && (!ssimComputed || ssimPass); }
  bool Escalation() const { return !numericPass && divergenceFrame > 0; }
};

struct GateRun {
  bool fatal = false;
  std::string fatalError;
  std::vector<CaptureResult> captures;
};

bool IsSsimLine(const std::string &line) {
  return line.compare(0, 8, "channel ") == 0;
}

bool ParseSsimLine(const std::string &line, double &minSsim, double &meanSsim) {
  static const std::string ssimMarker = "ssim min=";
  if (!IsSsimLine(line)) {
    return false;
  }
  const std::size_t markerAt = line.find(ssimMarker);
  if (markerAt == std::string::npos) {
    return false;
  }
  if (std::sscanf(line.c_str() + markerAt, "ssim min=%lf mean=%lf", &minSsim, &meanSsim) != 2) {
    return false;
  }
  return true;
}

std::string QuoteArgument(const std::string &value) {
#if defined(_WIN32)
  std::string quoted = "\"";
  for (char character : value) {
    if (character == '"') {
      quoted += "\\\"";
    } else {
      quoted.push_back(character);
    }
  }
  quoted.push_back('"');
  return quoted;
#else
  std::string quoted = "'";
  for (char character : value) {
    if (character == '\'') {
      quoted += "'\\''";
    } else {
      quoted.push_back(character);
    }
  }
  quoted.push_back('\'');
  return quoted;
#endif
}

std::string BuildSpawnCommand(const std::string &exe, const fs::path &a, const fs::path &b,
                              int tolerance, const fs::path &compositePath) {
  std::string command = QuoteArgument(exe) + " " + QuoteArgument(a.string()) + " " +
                        QuoteArgument(b.string()) + " --tolerance " +
                        std::to_string(tolerance);
  if (!compositePath.empty()) {
    command += " --composite-out " + QuoteArgument(compositePath.string());
  }
  command += " 2>&1";
#if defined(_WIN32)
  return "\"" + command + "\"";
#else
  return command;
#endif
}

bool RunFrameCompare(const std::string &exe, const fs::path &a, const fs::path &b, int tolerance,
                     CaptureSsim &ssim, std::string &error,
                     const fs::path &compositePath = fs::path(), FrameSsim *outFrame = nullptr) {
  const std::string command = BuildSpawnCommand(exe, a, b, tolerance, compositePath);
  std::string diagnostics;
  std::FILE *stream = SYNESTHESIA_VIBE_POPEN(command.c_str(), "r");
  if (stream == nullptr) {
    error = "failed to spawn '" + exe + "'";
    return false;
  }
  std::size_t channelLines = 0;
  double frameMeanSum = 0.0;
  double frameMin = 1.0;
  char buffer[1024];
  while (std::fgets(buffer, sizeof(buffer), stream) != nullptr) {
    const std::string line = buffer;
    if (IsSsimLine(line)) {
      double minSsim = 0.0;
      double meanSsim = 0.0;
      if (!ParseSsimLine(line, minSsim, meanSsim)) {
        diagnostics += line;
        continue;
      }
      ++channelLines;
      frameMeanSum += meanSsim;
      if (minSsim < frameMin) {
        frameMin = minSsim;
      }
      ssim.meanSum += meanSsim;
      ++ssim.samples;
      if (minSsim < ssim.minValue) {
        ssim.minValue = minSsim;
      }
      continue;
    }
    diagnostics += line;
  }
  const int status = SYNESTHESIA_VIBE_PCLOSE(stream);
  while (!diagnostics.empty() && (diagnostics.back() == '\n' || diagnostics.back() == '\r')) {
    diagnostics.pop_back();
  }
  if (status != 0) {
    error = "frame-compare exited with status " + std::to_string(status) + " for '" + a.string() +
            "' vs '" + b.string() + "'" +
            (diagnostics.empty() ? std::string() : (" -- " + diagnostics));
    return false;
  }
  if (channelLines != kSsimChannelsPerPair) {
    error = "expected " + std::to_string(kSsimChannelsPerPair) + " channel lines from '" + exe +
            "', got " + std::to_string(channelLines) + " for '" + a.string() + "' vs '" + b.string() +
            "'";
    return false;
  }
  ++ssim.pairs;
  if (outFrame != nullptr) {
    outFrame->meanSsim = frameMeanSum / static_cast<double>(kSsimChannelsPerPair);
    outFrame->minSsim = frameMin;
  }
  return true;
}

std::vector<std::string> EffectiveExcludes(const std::vector<std::string> &requested) {
  std::vector<std::string> result;
  result.push_back(kAlwaysExcludedField);
  for (const std::string &name : requested) {
    if (std::find(result.begin(), result.end(), name) == result.end()) {
      result.push_back(name);
    }
  }
  return result;
}

GateRun CollectGate(const fs::path &goldenRoot, const fs::path &actualRoot,
                    const std::vector<std::string> &requestedCaptures, int framesLimit,
                    bool exactMode, const std::vector<std::string> &requestedExcludes,
                    bool withSsim, const std::string &frameCompareExe) {
  GateRun run;
  const std::vector<std::string> excluded = EffectiveExcludes(requestedExcludes);

  std::vector<std::string> captureIds;
  if (requestedCaptures.empty()) {
    captureIds = ListCaptureIds(goldenRoot);
    if (captureIds.empty()) {
      run.fatal = true;
      run.fatalError = "no captures found under '" + goldenRoot.string() + "'";
      return run;
    }
  } else {
    for (const std::string &id : requestedCaptures) {
      const fs::path candidate = goldenRoot / id;
      std::error_code code;
      if (!fs::is_directory(candidate, code) || code) {
        run.fatal = true;
        run.fatalError = "capture not found under '" + goldenRoot.string() + "': " + id;
        return run;
      }
      captureIds.push_back(id);
    }
  }

  if (withSsim && !fs::exists(frameCompareExe)) {
    run.fatal = true;
    run.fatalError = "frame-compare executable not found: " + frameCompareExe;
    return run;
  }

  std::error_code code;
  if (!fs::is_directory(actualRoot, code) || code) {
    run.fatal = true;
    run.fatalError = "actual directory not found: " + actualRoot.string();
    return run;
  }

  for (const std::string &id : captureIds) {
    CaptureResult capture;
    capture.id = id;

    std::vector<int> goldenFrames = ListFrameNumbers(goldenRoot / id);
    if (goldenFrames.empty()) {
      run.fatal = true;
      run.fatalError = "no frames found under '" + (goldenRoot / id).string() + "'";
      return run;
    }
    if (framesLimit > 0 && goldenFrames.size() > static_cast<std::size_t>(framesLimit)) {
      goldenFrames.resize(static_cast<std::size_t>(framesLimit));
    }
    const std::vector<int> actualFrames = ListFrameNumbers(actualRoot / id);
    for (int number : actualFrames) {
      if (std::find(goldenFrames.begin(), goldenFrames.end(), number) == goldenFrames.end()) {
        ++capture.unpairedFrames;
      }
    }

    std::vector<FramePair> pairs;
    for (int number : goldenFrames) {
      FramePair pair;
      pair.number = number;
      pair.golden = goldenRoot / id / FrameFileName(number, kJsonSuffix);
      pair.actual = actualRoot / id / FrameFileName(number, kJsonSuffix);
      pairs.push_back(pair);
    }

    CompareContext context;
    context.exactMode = exactMode;
    context.excluded = excluded;

    for (const FramePair &pair : pairs) {
      std::error_code existsCode;
      if (!fs::is_regular_file(pair.actual, existsCode) || existsCode) {
        if (!capture.divergenceFound) {
          capture.divergenceFound = true;
          capture.shapeDivergence = true;
          capture.numericPass = false;
          capture.divergenceFrame = pair.number;
          capture.divergencePath = "(frame file)";
          capture.divergenceDetail = "actual frame missing: " + pair.actual.string();
        }
        continue;
      }
      JsonParser goldenParser("");
      std::string parseError;
      std::string goldenText;
      if (!ReadTextFile(pair.golden, goldenText, parseError)) {
        run.fatal = true;
        run.fatalError = parseError;
        return run;
      }
      JsonParser goldenParsed(goldenText);
      if (!goldenParsed.Parse()) {
        run.fatal = true;
        run.fatalError = "malformed JSON in '" + pair.golden.string() + "': " + goldenParsed.Error();
        return run;
      }
      std::string actualText;
      if (!ReadTextFile(pair.actual, actualText, parseError)) {
        run.fatal = true;
        run.fatalError = parseError;
        return run;
      }
      JsonParser actualParsed(actualText);
      if (!actualParsed.Parse()) {
        run.fatal = true;
        run.fatalError =
            "malformed JSON in '" + pair.actual.string() + "': " + actualParsed.Error();
        return run;
      }

      ++capture.framesCompared;

      CompareContext frameContext;
      frameContext.exactMode = exactMode;
      frameContext.excluded = excluded;
      if (!CheckGoldenInvariants(goldenParsed, frameContext)) {
        if (!capture.divergenceFound) {
          capture.divergenceFound = true;
          capture.shapeDivergence = true;
          capture.numericPass = false;
          capture.divergenceFrame = pair.number;
          capture.divergencePath = frameContext.divergence.path;
          capture.divergenceDetail = frameContext.divergence.detail;
        }
      } else {
        const bool ok = CompareValues(goldenParsed, goldenParsed.Root(), actualParsed,
                                      actualParsed.Root(), "", true, frameContext);
        if (!ok && !capture.divergenceFound) {
          capture.divergenceFound = true;
          capture.shapeDivergence = frameContext.divergence.shape;
          capture.nonFiniteDivergence = frameContext.divergence.nonFinite;
          capture.numericPass = false;
          capture.divergenceFrame = pair.number;
          capture.divergencePath = frameContext.divergence.path;
          capture.divergenceDetail = frameContext.divergence.detail;
        }
      }

      if (withSsim) {
        const fs::path goldenPng = (goldenRoot / id / FrameFileName(pair.number, kPngSuffix));
        const fs::path actualPng = (actualRoot / id / FrameFileName(pair.number, kPngSuffix));
        std::string ssimError;
        if (!RunFrameCompare(frameCompareExe, goldenPng, actualPng, kSsimAbsDiffTolerance,
                             capture.ssim, ssimError)) {
          run.fatal = true;
          run.fatalError = ssimError;
          return run;
        }
      }
    }

    if (capture.unpairedFrames > 0 && !capture.divergenceFound) {
      capture.divergenceFound = true;
      capture.shapeDivergence = true;
      capture.numericPass = false;
      capture.divergenceFrame = -1;
      capture.divergencePath = "(frame set)";
      capture.divergenceDetail = std::to_string(capture.unpairedFrames) +
                                 " actual frame(s) have no golden counterpart";
    }

    if (capture.framesCompared == 0) {
      run.fatal = true;
      run.fatalError = "no frames compared for capture '" + id + "'";
      return run;
    }

    if (withSsim) {
      capture.ssimComputed = true;
      const double meanSsim =
          capture.ssim.samples > 0
              ? capture.ssim.meanSum / static_cast<double>(capture.ssim.samples)
              : 0.0;
      capture.ssimPass = capture.ssim.samples > 0 && meanSsim >= kDefaultMeanSsim &&
                         capture.ssim.minValue >= kDefaultMinSsim;
    }

    run.captures.push_back(std::move(capture));
  }

  return run;
}

int ExitCodeFor(const GateRun &run, bool withSsim) {
  if (run.fatal) {
    return kExitError;
  }
  for (const CaptureResult &capture : run.captures) {
    if (!capture.numericPass) {
      return kExitDiverge;
    }
    if (withSsim && capture.ssimComputed && !capture.ssimPass) {
      return kExitDiverge;
    }
  }
  return kExitPass;
}

std::string DescribeFirstDivergence(const CaptureResult &capture) {
  if (!capture.divergenceFound) {
    return "-";
  }
  std::string text = "frame=" + std::to_string(capture.divergenceFrame) + " field=" +
                     capture.divergencePath;
  if (!capture.divergenceDetail.empty()) {
    text += " " + capture.divergenceDetail;
  }
  return text;
}

std::string MarkdownCell(const std::string &value) {
  std::string escaped;
  for (char character : value) {
    if (character == '|') {
      escaped += "\\|";
    } else if (character == '\n') {
      escaped.push_back(' ');
    } else {
      escaped.push_back(character);
    }
  }
  return escaped;
}

std::string FormatSsim(double value) {
  char buffer[32];
  std::snprintf(buffer, sizeof(buffer), "%.6f", value);
  return buffer;
}

std::string MatrixRows(const GateRun &run) {
  std::string rows;
  for (const CaptureResult &capture : run.captures) {
    const double meanSsim = capture.ssim.samples > 0
                                ? capture.ssim.meanSum / static_cast<double>(capture.ssim.samples)
                                : 0.0;
    const std::string numericVerdict = capture.numericPass ? "PASS" : "FAIL";
    const std::string ssimVerdict = capture.ssimPass ? "PASS" : "FAIL";
    const std::string overall = capture.OverallPass() ? "PASS" : "FAIL";
    rows += "| " + MarkdownCell(capture.id) + " | " + std::to_string(capture.framesCompared) +
            " | " + numericVerdict + " | " + MarkdownCell(DescribeFirstDivergence(capture)) +
            " | " + FormatSsim(meanSsim) + " | " + FormatSsim(capture.ssim.minValue) + " | " +
            ssimVerdict + " | " + overall + " | default (mean>=0.98, min>=0.95) |\n";
  }
  return rows;
}

std::string MatrixHeader() {
  return "| Capture | Frames | Numeric | First divergence | SSIM mean | SSIM min | SSIM verdict | "
         "Overall | Thresholds |\n"
         "|---|---|---|---|---|---|---|---|---|\n";
}

std::string RenderMatrix(const GateRun &run, const fs::path &goldenRoot, const fs::path &actualRoot) {
  std::string text;
  text += "# Phase 2 per-preset matrix\n\n";
  text += "Generated by `state-compare matrix`. Golden: `" + goldenRoot.string() + "`. Actual: `" +
          actualRoot.string() + "`.\n\n";
  text += "Aggregation (pinned, C2-011): the preset **mean** is the arithmetic mean of every "
          "per-frame per-channel `mean` reported by `frame-compare`; the preset **min** is the "
          "minimum of every per-frame per-channel `min`. Over 600 frames that is "
          "1800 values per preset. Alpha is ignored, matching `frame-compare`.\n\n";
  text += "PASS per preset requires mean SSIM >= 0.98 **and** min SSIM >= 0.95. Per-channel "
          "abs-diff is reported by the underlying tool but is not gated here.\n\n";
  text += MatrixHeader();
  text += MatrixRows(run);
  return text;
}

std::string RenderReport(const GateRun &run, const fs::path &goldenRoot, const fs::path &actualRoot,
                         const std::vector<std::string> &excluded, bool exactMode, int framesLimit,
                         bool withSsim, const std::vector<std::string> &commandLine) {
  std::string text;
  text += "# P2 gate report\n\n";
  text += "## Policies in force\n\n";
  text += "- **C2-002 (field exclusion).** `passes` is always excluded from the golden compare. "
          "The GL dump emits `passes[]` as named uniforms, and the named-block comparison is "
          "P2-091's, not this tool's. Effective excluded top-level fields: ";
  for (std::size_t index = 0; index < excluded.size(); ++index) {
    if (index > 0) {
      text += ", ";
    }
    text += "`" + excluded[index] + "`";
  }
  text += ".\n";
  text += "- **C2-012 (numeric tolerance).** `hex` is compared first: bit-identical is an "
          "immediate PASS. Otherwise a value PASSes when `|a-b| <= 1e-5` or when "
          "`|a| > 1 && |a-b|/|a| <= 1e-5`. `dec` is never compared. A NaN/Inf value PASSes only "
          "on exact hex bit-identity. Mode for this run: `" +
          std::string(exactMode ? "exact" : "tolerance") + "`";
  if (exactMode) {
    text += " (bit-identity required, tolerance disabled)";
  }
  text += ".\n";
  text += "- **C2-011 (SSIM thresholds).** Preset PASS requires mean SSIM >= 0.98 **and** min "
          "SSIM >= 0.95 over all compared frames. Per-channel abs-diff is reported by "
          "`frame-compare` but is not gated: `frame-compare` itself is invoked with `--tolerance " +
          std::to_string(kSsimAbsDiffTolerance) +
          "` so a pixel diff can never turn a pair into a hard error here -- any non-zero "
          "`frame-compare` exit is reported as an error against the capture instead. No "
          "threshold overrides were applied.\n";
  text += "- **C2-011 aggregation (pinned).** The preset **mean** is the arithmetic mean of every "
          "per-frame per-channel `mean` value reported by `frame-compare`; the preset **min** is "
          "the minimum of every per-frame per-channel `min` value. Over 600 frames that is 1800 "
          "values per preset. **Alpha is ignored**, matching `frame-compare`.\n";
  text += "- **First-divergence reporting.** On failure the first divergence is reported as the "
          "capture, the frame number and the JSON field path. A divergence whose first frame is "
          "N > 0 (with every earlier frame exact) is listed as an escalation below: it is a "
          "late-onset divergence, not a frame-0 immediate mismatch.\n\n";

  text += "## Scope\n\n";
  text += "- Golden: `" + goldenRoot.string() + "`\n";
  text += "- Actual: `" + actualRoot.string() + "`\n";
  text += "- Frames per capture: " +
          (framesLimit > 0 ? std::to_string(framesLimit) : std::string("all")) + "\n";
  text += "- SSIM computed: " + std::string(withSsim ? "yes" : "no") + "\n\n";

  text += "## Per-capture\n\n";
  text += MatrixHeader();
  text += MatrixRows(run);

  text += "\n## Escalations\n\n";
  bool anyEscalation = false;
  for (const CaptureResult &capture : run.captures) {
    if (!capture.Escalation()) {
      continue;
    }
    anyEscalation = true;
    text += "- `" + capture.id + "`: first divergence at frame " +
            std::to_string(capture.divergenceFrame) + " (field `" + capture.divergencePath +
            "`) with frames 0.." + std::to_string(capture.divergenceFrame - 1) + " exact.\n";
  }
  if (!anyEscalation) {
    text += "- (none)\n";
  }

  text += "\n## Divergence detail\n\n";
  bool anyDivergence = false;
  for (const CaptureResult &capture : run.captures) {
    if (capture.numericPass) {
      continue;
    }
    anyDivergence = true;
    text += "- `" + capture.id + "`: " + DescribeFirstDivergence(capture) +
            (capture.shapeDivergence ? " [shape divergence]" : "") +
            (capture.nonFiniteDivergence ? " [non-finite]" : "") + "\n";
  }
  if (!anyDivergence) {
    text += "- (none)\n";
  }

  text += "\n## Commands actually run\n\n```\n";
  for (const std::string &argument : commandLine) {
    text += QuoteArgument(argument) + " ";
  }
  text += "\n```\n";
  return text;
}

std::string EscapeJsonString(const std::string &value) {
  std::string escaped;
  for (char character : value) {
    switch (character) {
      case '"': escaped += "\\\""; break;
      case '\\': escaped += "\\\\"; break;
      case '\n': escaped += "\\n"; break;
      case '\r': escaped += "\\r"; break;
      case '\t': escaped += "\\t"; break;
      default: escaped.push_back(character); break;
    }
  }
  return escaped;
}

std::string RenderCompareJson(const GateRun &run, const std::vector<std::string> &excluded,
                              const std::string &mode) {
  std::string text;
  text += "{\n";
  text += "  \"mode\": \"" + EscapeJsonString(mode) + "\",\n";
  text += "  \"excluded\": [";
  for (std::size_t index = 0; index < excluded.size(); ++index) {
    if (index > 0) {
      text += ", ";
    }
    text += "\"" + EscapeJsonString(excluded[index]) + "\"";
  }
  text += "],\n";
  text += "  \"captures\": [\n";
  for (std::size_t index = 0; index < run.captures.size(); ++index) {
    const CaptureResult &capture = run.captures[index];
    text += "    {\"capture\": \"" + EscapeJsonString(capture.id) + "\", \"frames\": " +
            std::to_string(capture.framesCompared) + ", \"verdict\": \"" +
            (capture.numericPass ? "PASS" : "FAIL") + "\", \"shapeDivergence\": " +
            (capture.shapeDivergence ? "true" : "false") + ", \"firstDivergenceFrame\": ";
    if (capture.divergenceFound && capture.divergenceFrame >= 0) {
      text += std::to_string(capture.divergenceFrame);
    } else {
      text += "null";
    }
    text += ", \"firstDivergencePath\": ";
    if (capture.divergenceFound) {
      text += "\"" + EscapeJsonString(capture.divergencePath) + "\"";
    } else {
      text += "null";
    }
    text += ", \"escalation\": " + std::string(capture.Escalation() ? "true" : "false") + "}";
    if (index + 1 < run.captures.size()) {
      text += ",";
    }
    text += "\n";
  }
  text += "  ],\n";
  text += "  \"result\": \"";
  const int code = ExitCodeFor(run, false);
  text += code == kExitPass ? "PASS" : (code == kExitDiverge ? "FAIL" : "ERROR");
  text += "\"\n}\n";
  return text;
}

VisualRun CollectVisual(const fs::path &goldenRoot, const fs::path &actualRoot,
                        const std::vector<std::string> &requestedCaptures,
                        const std::string &frameCompareExe, const fs::path &reportDir) {
  VisualRun run;

  std::vector<std::string> captureIds;
  if (requestedCaptures.empty()) {
    captureIds = ListCaptureIds(goldenRoot);
    if (captureIds.empty()) {
      run.fatal = true;
      run.fatalError = "no captures found under '" + goldenRoot.string() + "'";
      return run;
    }
  } else {
    for (const std::string &id : requestedCaptures) {
      const fs::path candidate = goldenRoot / id;
      std::error_code code;
      if (!fs::is_directory(candidate, code) || code) {
        run.fatal = true;
        run.fatalError = "capture not found under '" + goldenRoot.string() + "': " + id;
        return run;
      }
      captureIds.push_back(id);
    }
  }

  if (!fs::exists(frameCompareExe)) {
    run.fatal = true;
    run.fatalError = "frame-compare executable not found: " + frameCompareExe;
    return run;
  }

  std::error_code code;
  if (!fs::is_directory(actualRoot, code) || code) {
    run.fatal = true;
    run.fatalError = "actual directory not found: " + actualRoot.string();
    return run;
  }

  for (const std::string &id : captureIds) {
    VisualCapture capture;
    capture.id = id;

    const fs::path goldenDir = goldenRoot / id;
    const std::vector<int> goldenFrames = ListFrameNumbers(goldenDir, kPngSuffix);
    if (goldenFrames.empty()) {
      run.fatal = true;
      run.fatalError = "no golden PNG frames found under '" + goldenDir.string() + "'";
      return run;
    }

    const fs::path actualDir = actualRoot / id;
    const std::vector<int> actualFrames = ListFrameNumbers(actualDir, kPngSuffix);
    std::vector<int> frames;
    std::set_intersection(goldenFrames.begin(), goldenFrames.end(), actualFrames.begin(),
                          actualFrames.end(), std::back_inserter(frames));
    if (frames.empty()) {
      run.fatal = true;
      run.fatalError = "no actual PNG frames found under '" + actualDir.string() + "'";
      return run;
    }

    for (int number : frames) {
      FrameSsim frame;
      frame.number = number;

      const fs::path goldenPng = goldenDir / FrameFileName(number, kPngSuffix);
      const fs::path actualPng = actualDir / FrameFileName(number, kPngSuffix);
      const fs::path compositePath = actualDir / FrameFileName(number, kCompositeSuffix);

      std::error_code relativeCode;
      const fs::path relative = fs::relative(compositePath, reportDir, relativeCode);
      frame.composite =
          (!relativeCode && !relative.empty() ? relative : compositePath).generic_string();

      CaptureSsim ssim;
      FrameSsim measured;
      std::string compareError;
      if (!RunFrameCompare(frameCompareExe, goldenPng, actualPng, kSsimAbsDiffTolerance, ssim,
                           compareError, compositePath, &measured)) {
        frame.error = compareError;
        ++capture.errors;
        capture.frames.push_back(frame);
        continue;
      }

      frame.meanSsim = measured.meanSsim;
      frame.minSsim = measured.minSsim;
      capture.hasSsim = true;
      capture.meanSum += measured.meanSsim;
      if (measured.minSsim < capture.minValue) {
        capture.minValue = measured.minSsim;
      }
      capture.frames.push_back(frame);
    }

    run.captures.push_back(capture);
  }

  return run;
}

std::size_t VisualFrameCount(const VisualRun &run) {
  std::size_t total = 0;
  for (const VisualCapture &capture : run.captures) {
    total += capture.frames.size();
  }
  return total;
}

std::size_t VisualErrorCount(const VisualRun &run) {
  std::size_t total = 0;
  for (const VisualCapture &capture : run.captures) {
    total += capture.errors;
  }
  return total;
}

std::string VisualFrameRows(const VisualRun &run) {
  std::string rows;
  for (const VisualCapture &capture : run.captures) {
    for (const FrameSsim &frame : capture.frames) {
      const std::string frameName = FrameFileName(frame.number, kCompositeSuffix);
      rows += "| " + MarkdownCell(capture.id) + " | " + std::to_string(frame.number) + " | ";
      if (frame.error.empty()) {
        rows += FormatSsim(frame.meanSsim) + " | " + FormatSsim(frame.minSsim) + " | [" +
                frameName + "](" + frame.composite + ") | - |\n";
      } else {
        rows += "- | - | - | " + MarkdownCell("not compared: " + frame.error) + " |\n";
      }
    }
  }
  return rows;
}

std::string VisualSummaryRows(const VisualRun &run) {
  std::string rows;
  for (const VisualCapture &capture : run.captures) {
    const double mean = capture.hasSsim
                            ? capture.meanSum / static_cast<double>(capture.frames.size())
                            : 0.0;
    rows += "| " + MarkdownCell(capture.id) + " | " + std::to_string(capture.frames.size()) +
            " | " + std::to_string(capture.errors) + " | " +
            (capture.hasSsim ? FormatSsim(mean) : "-") + " | " +
            (capture.hasSsim ? FormatSsim(capture.minValue) : "-") + " |\n";
  }
  return rows;
}

std::string RenderVisualReport(const VisualRun &run, const fs::path &goldenRoot,
                               const fs::path &actualRoot, const std::string &frameCompareExe,
                               const std::vector<std::string> &commandLine) {
  std::string text;
  text += "# Visual check report\n\n";
  text += "## What this is\n\n";
  text += "A sampled, visual-only check: for every requested preset it pairs each golden PNG with "
          "the GL capture's PNG of the same frame, prints the frame's SSIM, and writes a 3-panel "
          "composite (`golden | GL | amplified abs-diff`) beside the actual PNG for eyeballing. "
          "Only frames present as a PNG in both trees are paired, so a sampled capture reports one "
          "row per sampled frame rather than one row per golden frame.\n\n";
  text += "- **SSIM is a similarity score, not a correctness oracle.** The 0.98 mean / 0.95 min "
          "thresholds of C2-011 are **not enforced** here (CW-005 recommendation (a)); the "
          "numbers are reported so a human can watch them rise as the render work lands.\n";
  text += "- **The actual column is the GL backend.** Scores below the 0.98/0.95 gate are "
          "reported, not failed; mid-run divergence in audio-reactive feedback presets is "
          "expected to grow with frame number, so read the composites before trusting a number.\n";
  text += "- Alpha is ignored, matching `frame-compare`.\n\n";

  text += "## Scope\n\n";
  text += "- Golden: `" + goldenRoot.string() + "`\n";
  text += "- Actual: `" + actualRoot.string() + "`\n";
  text += "- frame-compare: `" + frameCompareExe + "`\n";
  text += "- Presets: " + std::to_string(run.captures.size()) + ", frames compared: " +
          std::to_string(VisualFrameCount(run)) + ", frame errors: " +
          std::to_string(VisualErrorCount(run)) + "\n";
  text += "- Composite path convention: `<actual>/<capture>/frame_NNNN-composite.png`\n\n";

  text += "## Per-frame SSIM\n\n";
  text += "| Capture | Frame | SSIM mean | SSIM min | Composite | Note |\n";
  text += "|---|---|---|---|---|---|\n";
  text += VisualFrameRows(run);

  text += "\n## Per-preset summary\n\n";
  text += "| Capture | Frames | Frame errors | SSIM mean | SSIM min |\n";
  text += "|---|---|---|---|---|\n";
  text += VisualSummaryRows(run);

  text += "\n## Commands actually run\n\n```\n";
  for (const std::string &argument : commandLine) {
    text += QuoteArgument(argument) + " ";
  }
  text += "\n```\n";
  return text;
}

enum class Command { None, Compare, Matrix, Report, Visual };

struct Options {
  Command command = Command::None;
  std::string golden;
  std::string actual;
  std::string frameCompare;
  std::string out;
  std::string matrix;
  std::string jsonOut;
  std::vector<std::string> captures;
  std::vector<std::string> excludes;
  std::string mode = "tolerance";
  int frames = 0;
  bool help = false;
  std::vector<std::string> commandLine;
};

void PrintUsage(const char *programName) {
  std::fprintf(stderr,
      "Usage: %s compare --golden <dir> --actual <dir>\n"
      "                          [--capture <id>]... [--frames N] [--mode tolerance|exact]\n"
      "                          [--exclude <field>]... [--json-out <path>]\n"
      "       %s matrix  --golden <dir> --actual <dir> --frame-compare <exe>\n"
      "                          [--out <path>] [--capture <id>]... [--frames N]\n"
      "       %s report  --golden <dir> --actual <dir> --frame-compare <exe> --out <dir>\n"
      "                          [--capture <id>]... [--frames N] [--matrix <path>]\n"
      "       %s visual  --golden <dir> --actual <dir> --frame-compare <exe> --out <dir>\n"
      "                          [--capture <id>]...\n\n"
      "Compares capture-mode frame-state JSON (tests/golden/schema.md) between a golden tree and an\n"
      "actual tree, frame_NNNN.json paired by name in numeric frame order. Floats are compared on\n"
      "their decoded hex bits, never on the 'dec' text. 'passes' is always excluded (C2-002).\n"
      "The 'matrix' and 'report' subcommands also compute SSIM by invoking the frame-compare\n"
      "executable once per frame pair; the SSIM math lives only in that tool.\n\n"
      "Exit codes:\n"
      "  0   every compared frame PASSed\n"
      "  1   at least one divergence, or a missing/unpaired frame\n"
      "  2   usage error, unreadable input, or malformed JSON\n\n"
      "The 'visual' subcommand pairs golden and actual PNGs per frame (JSON is not required, so it\n"
      "works on a sampled, PNG-only capture), invokes frame-compare once per pair to print SSIM and\n"
      "write a '<frame>-composite.png' beside each actual PNG, and writes report.md into --out. Only\n"
      "frames present as a PNG in both trees are compared, so a sampled capture yields one row per\n"
      "sampled frame. SSIM is reported, never gated (CW-005 recommendation (a)).\n",
      programName, programName, programName, programName);
}

bool TakeValue(int argc, char **argv, int &index, const std::string &flag, std::string &out,
               std::string &error) {
  if (index + 1 >= argc) {
    error = flag + " requires a value";
    return false;
  }
  out = argv[++index];
  return true;
}

bool ParseArgs(int argc, char **argv, Options &options, std::string &error) {
  for (int index = 0; index < argc; ++index) {
    options.commandLine.push_back(argv[index]);
  }

  if (argc < 2) {
    error = "a subcommand is required";
    return false;
  }
  const std::string subcommand = argv[1];
  if (subcommand == "--help" || subcommand == "-h") {
    options.help = true;
    return true;
  }
  if (subcommand == "compare") {
    options.command = Command::Compare;
  } else if (subcommand == "matrix") {
    options.command = Command::Matrix;
  } else if (subcommand == "report") {
    options.command = Command::Report;
  } else if (subcommand == "visual") {
    options.command = Command::Visual;
  } else {
    error = "unknown subcommand '" + subcommand + "'";
    return false;
  }

  for (int index = 2; index < argc; ++index) {
    const std::string flag = argv[index];
    if (flag == "--help" || flag == "-h") {
      options.help = true;
      return true;
    }
    if (flag == "--golden") {
      if (!TakeValue(argc, argv, index, flag, options.golden, error)) {
        return false;
      }
    } else if (flag == "--actual") {
      if (!TakeValue(argc, argv, index, flag, options.actual, error)) {
        return false;
      }
    } else if (flag == "--frame-compare") {
      if (!TakeValue(argc, argv, index, flag, options.frameCompare, error)) {
        return false;
      }
    } else if (flag == "--out") {
      if (!TakeValue(argc, argv, index, flag, options.out, error)) {
        return false;
      }
    } else if (flag == "--matrix") {
      if (!TakeValue(argc, argv, index, flag, options.matrix, error)) {
        return false;
      }
    } else if (flag == "--json-out") {
      if (!TakeValue(argc, argv, index, flag, options.jsonOut, error)) {
        return false;
      }
    } else if (flag == "--mode") {
      if (!TakeValue(argc, argv, index, flag, options.mode, error)) {
        return false;
      }
    } else if (flag == "--capture") {
      std::string value;
      if (!TakeValue(argc, argv, index, flag, value, error)) {
        return false;
      }
      options.captures.push_back(value);
    } else if (flag == "--exclude") {
      std::string value;
      if (!TakeValue(argc, argv, index, flag, value, error)) {
        return false;
      }
      options.excludes.push_back(value);
    } else if (flag == "--frames") {
      std::string value;
      if (!TakeValue(argc, argv, index, flag, value, error)) {
        return false;
      }
      char *end = nullptr;
      const long parsed = std::strtol(value.c_str(), &end, 10);
      if (end == nullptr || *end != '\0' || parsed < 0) {
        error = "--frames requires a non-negative integer";
        return false;
      }
      options.frames = static_cast<int>(parsed);
    } else {
      error = "unknown option '" + flag + "'";
      return false;
    }
  }

  if (options.mode != "tolerance" && options.mode != "exact") {
    error = "--mode must be 'tolerance' or 'exact'";
    return false;
  }
  if (options.golden.empty()) {
    error = "--golden is required";
    return false;
  }
  if (options.actual.empty()) {
    error = "--actual is required";
    return false;
  }
  if (options.command == Command::Matrix || options.command == Command::Report ||
      options.command == Command::Visual) {
    if (options.frameCompare.empty()) {
      error = "--frame-compare is required";
      return false;
    }
  }
  if ((options.command == Command::Report || options.command == Command::Visual) &&
      options.out.empty()) {
    error = "--out is required";
    return false;
  }
  return true;
}

int RunCompare(const Options &options) {
  if (!fs::is_directory(options.golden)) {
    std::fprintf(stderr, "error: golden directory not found: %s\n", options.golden.c_str());
    return kExitError;
  }
  const GateRun run = CollectGate(options.golden, options.actual, options.captures, options.frames,
                                  options.mode == "exact", options.excludes, false, "");
  if (run.fatal) {
    std::fprintf(stderr, "error: %s\n", run.fatalError.c_str());
    return kExitError;
  }

  for (const CaptureResult &capture : run.captures) {
    std::printf("capture %s: %s frames=%d", capture.id.c_str(),
                capture.numericPass ? "PASS" : "FAIL", capture.framesCompared);
    if (!capture.numericPass) {
      std::printf(" first-divergence %s", DescribeFirstDivergence(capture).c_str());
    }
    std::printf("\n");
    if (capture.Escalation()) {
      std::printf("escalation %s: first divergence at frame %d, frames 0..%d exact\n",
                  capture.id.c_str(), capture.divergenceFrame, capture.divergenceFrame - 1);
    }
  }

  const std::vector<std::string> excluded = EffectiveExcludes(options.excludes);
  std::size_t passed = 0;
  for (const CaptureResult &capture : run.captures) {
    if (capture.numericPass) {
      ++passed;
    }
  }
  std::printf("compare: captures=%zu pass=%zu fail=%zu mode=%s excluded=",
              run.captures.size(), passed, run.captures.size() - passed, options.mode.c_str());
  for (std::size_t index = 0; index < excluded.size(); ++index) {
    std::printf("%s%s", index > 0 ? "," : "", excluded[index].c_str());
  }
  std::printf("\n");

  const int code = ExitCodeFor(run, false);
  std::printf("result: %s\n", code == kExitPass ? "PASS" : "FAIL");

  if (!options.jsonOut.empty()) {
    std::string writeError;
    if (!WriteTextFile(options.jsonOut, RenderCompareJson(run, excluded, options.mode),
                       writeError)) {
      std::fprintf(stderr, "error: %s\n", writeError.c_str());
      return kExitError;
    }
  }
  return code;
}

int RunMatrix(const Options &options) {
  if (!fs::is_directory(options.golden)) {
    std::fprintf(stderr, "error: golden directory not found: %s\n", options.golden.c_str());
    return kExitError;
  }
  const GateRun run = CollectGate(options.golden, options.actual, options.captures, options.frames,
                                  options.mode == "exact", options.excludes, true,
                                  options.frameCompare);
  if (run.fatal) {
    std::fprintf(stderr, "error: %s\n", run.fatalError.c_str());
    return kExitError;
  }

  const std::string matrix = RenderMatrix(run, options.golden, options.actual);
  if (options.out.empty()) {
    std::printf("%s", matrix.c_str());
  } else {
    std::string writeError;
    if (!WriteTextFile(options.out, matrix, writeError)) {
      std::fprintf(stderr, "error: %s\n", writeError.c_str());
      return kExitError;
    }
    std::printf("matrix: %s\n", options.out.c_str());
  }

  for (const CaptureResult &capture : run.captures) {
    const double meanSsim = capture.ssim.samples > 0
                                ? capture.ssim.meanSum / static_cast<double>(capture.ssim.samples)
                                : 0.0;
    std::printf("capture %s: numeric=%s frames=%d ssim-mean=%s ssim-min=%s ssim=%s overall=%s\n",
                capture.id.c_str(), capture.numericPass ? "PASS" : "FAIL", capture.framesCompared,
                FormatSsim(meanSsim).c_str(), FormatSsim(capture.ssim.minValue).c_str(),
                capture.ssimPass ? "PASS" : "FAIL", capture.OverallPass() ? "PASS" : "FAIL");
  }

  const int code = ExitCodeFor(run, true);
  std::printf("result: %s\n", code == kExitPass ? "PASS" : "FAIL");
  return code;
}

int RunReport(const Options &options) {
  if (!fs::is_directory(options.golden)) {
    std::fprintf(stderr, "error: golden directory not found: %s\n", options.golden.c_str());
    return kExitError;
  }
  const GateRun run = CollectGate(options.golden, options.actual, options.captures, options.frames,
                                  options.mode == "exact", options.excludes, true,
                                  options.frameCompare);
  if (run.fatal) {
    std::fprintf(stderr, "error: %s\n", run.fatalError.c_str());
    return kExitError;
  }

  const std::vector<std::string> excluded = EffectiveExcludes(options.excludes);
  const fs::path reportPath = fs::path(options.out) / "report.md";
  const std::string report =
      RenderReport(run, options.golden, options.actual, excluded, options.mode == "exact",
                   options.frames, true, options.commandLine);
  std::string writeError;
  if (!WriteTextFile(reportPath, report, writeError)) {
    std::fprintf(stderr, "error: %s\n", writeError.c_str());
    return kExitError;
  }
  if (!options.matrix.empty()) {
    if (!WriteTextFile(options.matrix, RenderMatrix(run, options.golden, options.actual),
                       writeError)) {
      std::fprintf(stderr, "error: %s\n", writeError.c_str());
      return kExitError;
    }
  }

  for (const CaptureResult &capture : run.captures) {
    const double meanSsim = capture.ssim.samples > 0
                                ? capture.ssim.meanSum / static_cast<double>(capture.ssim.samples)
                                : 0.0;
    std::printf("capture %s: numeric=%s frames=%d", capture.id.c_str(),
                capture.numericPass ? "PASS" : "FAIL", capture.framesCompared);
    if (!capture.numericPass) {
      std::printf(" first-divergence %s", DescribeFirstDivergence(capture).c_str());
    }
    std::printf(" ssim-mean=%s ssim-min=%s ssim=%s overall=%s\n", FormatSsim(meanSsim).c_str(),
                FormatSsim(capture.ssim.minValue).c_str(), capture.ssimPass ? "PASS" : "FAIL",
                capture.OverallPass() ? "PASS" : "FAIL");
  }

  std::printf("report: %s\n", reportPath.string().c_str());
  if (!options.matrix.empty()) {
    std::printf("matrix: %s\n", options.matrix.c_str());
  }

  const int code = ExitCodeFor(run, true);
  std::printf("result: %s\n", code == kExitPass ? "PASS" : "FAIL");
  return code;
}

int RunVisual(const Options &options) {
  if (!fs::is_directory(options.golden)) {
    std::fprintf(stderr, "error: golden directory not found: %s\n", options.golden.c_str());
    return kExitError;
  }
  const VisualRun run = CollectVisual(options.golden, options.actual, options.captures,
                                      options.frameCompare, fs::path(options.out));
  if (run.fatal) {
    std::fprintf(stderr, "error: %s\n", run.fatalError.c_str());
    return kExitError;
  }

  const fs::path reportPath = fs::path(options.out) / "report.md";
  const std::string report = RenderVisualReport(run, options.golden, options.actual,
                                                options.frameCompare, options.commandLine);
  std::string writeError;
  if (!WriteTextFile(reportPath, report, writeError)) {
    std::fprintf(stderr, "error: %s\n", writeError.c_str());
    return kExitError;
  }

  for (const VisualCapture &capture : run.captures) {
    const double mean =
        capture.hasSsim ? capture.meanSum / static_cast<double>(capture.frames.size()) : 0.0;
    std::printf("capture %s: frames=%zu errors=%zu ssim-mean=%s ssim-min=%s\n",
                capture.id.c_str(), capture.frames.size(), capture.errors,
                capture.hasSsim ? FormatSsim(mean).c_str() : "-",
                capture.hasSsim ? FormatSsim(capture.minValue).c_str() : "-");
  }

  std::printf("visual: captures=%zu frames=%zu errors=%zu gate=off\n", run.captures.size(),
              VisualFrameCount(run), VisualErrorCount(run));
  std::printf("report: %s\n", reportPath.string().c_str());

  const bool hasError = VisualErrorCount(run) > 0;
  std::printf("result: %s\n", hasError ? "FAIL" : "PASS");
  return hasError ? kExitDiverge : kExitPass;
}

}  // namespace

int main(int argc, char **argv) {
  Options options;
  std::string error;
  if (!ParseArgs(argc, argv, options, error)) {
    std::fprintf(stderr, "error: %s\n", error.c_str());
    PrintUsage(argv[0]);
    return kExitError;
  }
  if (options.help) {
    PrintUsage(argv[0]);
    return kExitPass;
  }

  switch (options.command) {
    case Command::Compare: return RunCompare(options);
    case Command::Matrix: return RunMatrix(options);
    case Command::Report: return RunReport(options);
    case Command::Visual: return RunVisual(options);
    case Command::None: break;
  }
  PrintUsage(argv[0]);
  return kExitError;
}