#pragma once
// Simulador: só o que o painel (placa/painel_watcher.h) usa do Display do XiaoZhi; sem threads, a trava não faz nada
class Theme {
public:
    virtual ~Theme() = default;
};

class Display {
public:
    virtual ~Display() = default;
    virtual Theme* GetTheme() { return current_theme_; }
    Theme* current_theme_ = nullptr;
};

class DisplayLockGuard {
public:
    explicit DisplayLockGuard(Display*) {}
};
