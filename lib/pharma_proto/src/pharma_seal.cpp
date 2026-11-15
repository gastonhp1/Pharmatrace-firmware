#include "pharma_seal.h"

namespace pharma {

LoopState classifyLoop(uint16_t mv, const SealConfig& cfg) {
  if (mv < cfg.loopClosedMinMv) return LoopState::Shorted;
  if (mv > cfg.loopClosedMaxMv) return LoopState::Open;
  return LoopState::Closed;
}

bool SealMonitor::windowOpen(uint32_t nowMs) const {
  return windowArmed_ && uint32_t(nowMs - windowStartMs_) < cfg_.authWindowMs;
}

void SealMonitor::authorize(uint32_t nowMs) {
  windowArmed_ = true;
  windowStartMs_ = nowMs;
  // Una caja violada se "perdona" solo así: la violación ya quedó en lecturas firmadas.
  if (state_ == SealState::Breached) state_ = open_ ? SealState::AuthorizedOpen : SealState::Sealed;
}

uint8_t SealMonitor::update(const SealInputs& in, uint32_t nowMs) {
  intact_ = classifyLoop(in.loopMv, cfg_) == LoopState::Closed && in.fsrMv >= cfg_.fsrMinMv &&
            in.probePresent;
  const bool lidNow = in.probePresent && in.lux >= cfg_.luxOpen;
  const bool openNow = !intact_ || lidNow;

  // Antirrebote: un cambio vale recién cuando se sostiene debounceMs.
  if (openNow == open_) {
    pendingValid_ = false;
    // Ya estaba abierta (por ejemplo, el cinto) y ahora además entra luz: es un evento nuevo.
    const bool lidEdge = open_ && lidNow && !lidOpen_;
    lidOpen_ = lidNow && open_;
    return lidEdge ? FLAG_LID_OPENED : 0;
  }
  if (!pendingValid_ || pendingOpen_ != openNow) {
    pendingValid_ = true;
    pendingOpen_ = openNow;
    pendingSinceMs_ = nowMs;
    if (cfg_.debounceMs != 0) return 0;
  }
  if (uint32_t(nowMs - pendingSinceMs_) < cfg_.debounceMs) return 0;

  pendingValid_ = false;
  open_ = openNow;
  lidOpen_ = lidNow && open_;
  uint8_t events = 0;

  if (open_) {
    if (lidNow) events |= FLAG_LID_OPENED;
    if (state_ == SealState::Sealed) {
      if (windowOpen(nowMs)) {
        state_ = SealState::AuthorizedOpen;
        windowArmed_ = false;  // el permiso se gasta con una apertura
      } else {
        state_ = SealState::Breached;
      }
    }
  } else if (state_ == SealState::AuthorizedOpen) {
    state_ = SealState::Sealed;  // la cerraron: vuelve a vigilar
  }
  return events;
}

}  // namespace pharma
