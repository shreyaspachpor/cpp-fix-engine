#pragma once

#include "common/enums.h"

struct RiskResult
{
    RiskCheckType check_type=RiskCheckType::Approved;
    RejectionReason rejection_reason=RejectionReason::None;
};
