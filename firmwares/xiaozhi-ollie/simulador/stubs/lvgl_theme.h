#pragma once
// Simulador: tema mínimo com a coleção de emojis (os GIFs do mascote-clawd), como o LvglTheme do XiaoZhi
#include <memory>

#include "display.h"
#include "emoji_collection.h"

class LvglTheme : public Theme {
public:
    std::shared_ptr<EmojiCollection> emoji_collection() const { return emoji_collection_; }
    void set_emoji_collection(std::shared_ptr<EmojiCollection> c) { emoji_collection_ = c; }

private:
    std::shared_ptr<EmojiCollection> emoji_collection_;
};
