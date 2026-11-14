#include "json_stream.h"

#include <string.h>

void JsonStream::feed(const char *data, size_t len) {
  for (size_t i = 0; i < len && !err_; ++i) put(data[i]);
}

void JsonStream::append(char c) {
  if (len_ < sizeof(buf_) - 1) buf_[len_++] = c;
}

void JsonStream::put(char c) {
  if (state_ == State::String) {
    if (escape_) {
      escape_ = false;
      append(c);  // \uXXXX-et nem dekódolunk, a mezőinkben nem fordul elő
    } else if (c == '\\') {
      escape_ = true;
    } else if (c == '"') {
      state_ = State::Idle;
      endString();
    } else {
      append(c);
    }
    return;
  }

  if (state_ == State::Literal) {
    if (strchr(",}] \t\r\n", c) == nullptr) {
      append(c);
      return;
    }
    state_ = State::Idle;
    emitValue(false);
    // a határoló karaktert alább dolgozzuk fel
  }

  switch (c) {
    case '{':
    case '[':
      push(c == '{');
      break;
    case '}':
    case ']':
      pop();
      break;
    case '"':
      state_ = State::String;
      len_ = 0;
      break;
    case ',':
      expectKey_ = topIsObject();
      break;
    case ':':
    case ' ':
    case '\t':
    case '\r':
    case '\n':
      break;
    default:
      state_ = State::Literal;
      len_ = 0;
      append(c);
      break;
  }
}

void JsonStream::push(bool isObject) {
  if (depth_ >= kMaxDepth) {
    err_ = true;
    return;
  }
  const char *key = keyFor();
  stack_[depth_++] = isObject;
  seenRoot_ = true;
  h_.onStart(depth_, key, isObject);
  expectKey_ = isObject;
}

void JsonStream::pop() {
  if (depth_ == 0) {
    err_ = true;
    return;
  }
  h_.onEnd(depth_, stack_[depth_ - 1]);
  depth_--;
  expectKey_ = false;
}

void JsonStream::endString() {
  buf_[len_] = '\0';
  if (topIsObject() && expectKey_) {
    strncpy(key_, buf_, sizeof(key_) - 1);
    key_[sizeof(key_) - 1] = '\0';
    expectKey_ = false;
  } else {
    emitValue(true);
  }
}

void JsonStream::emitValue(bool isString) {
  buf_[len_] = '\0';
  h_.onValue(depth_, keyFor(), buf_, isString);
}
