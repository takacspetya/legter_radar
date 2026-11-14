#pragma once

#include <stddef.h>

// Minimális, folyamatos (SAX-szerű) JSON-feldolgozó.
// Darabonként kapja a bemenetet, így a teljes választ nem kell memóriában
// tartani. A túl hosszú kulcsokat és értékeket csonkolja.
class JsonStream {
 public:
  struct Handler {
    // depth: a beágyazási mélység (a gyökérobjektum 1).
    // key: a tag neve, ha a szülő objektum; tömbelemeknél üres.
    virtual void onStart(int depth, const char *key, bool isObject) {}
    virtual void onEnd(int depth, bool isObject) {}
    virtual void onValue(int depth, const char *key, const char *value,
                         bool isString) {}
    virtual ~Handler() = default;
  };

  explicit JsonStream(Handler &h) : h_(h) {}

  void feed(const char *data, size_t len);
  bool complete() const { return !err_ && depth_ == 0 && seenRoot_ && state_ == State::Idle; }
  bool error() const { return err_; }

 private:
  enum class State { Idle, String, Literal };
  static constexpr int kMaxDepth = 16;

  void put(char c);
  void push(bool isObject);
  void pop();
  void endString();
  void emitValue(bool isString);
  void append(char c);
  bool topIsObject() const { return depth_ > 0 && stack_[depth_ - 1]; }
  const char *keyFor() const { return topIsObject() ? key_ : ""; }

  Handler &h_;
  State state_ = State::Idle;
  bool stack_[kMaxDepth] = {};
  int depth_ = 0;
  bool expectKey_ = false;
  bool escape_ = false;
  bool err_ = false;
  bool seenRoot_ = false;
  char key_[24] = {};
  char buf_[48] = {};
  size_t len_ = 0;
};
