#ifndef ASYMMETRIC_LOSS_H
#define ASYMMETRIC_LOSS_H

#include <torch/torch.h>

// Penalizes overestimation w_over times more than underestimation.
// Biases the network toward underestimation so the heuristic stays admissible.
inline torch::Tensor asymmetric_mse(torch::Tensor pred, torch::Tensor target,
                                     float w_over = 4.f, float w_under = 1.f) {
  auto over  = torch::relu(pred - target);
  auto under = torch::relu(target - pred);
  return (w_over * over.pow(2) + w_under * under.pow(2)).mean();
}

#endif // ASYMMETRIC_LOSS_H
