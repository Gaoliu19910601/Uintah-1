#ifndef UINTAH_HOMEBREW_MPMICELABEL_H
#define UINTAH_HOMEBREW_MPMICELABEL_H

#include <Packages/Uintah/Core/Grid/VarLabel.h>
#include <vector>

using std::vector;

namespace Uintah {

    class MPMICELabel {
    public:

      MPMICELabel();
      ~MPMICELabel();

      const VarLabel* cMassLabel;
      const VarLabel* cVolumeLabel;
      const VarLabel* vel_CCLabel;
      const VarLabel* velstar_CCLabel;
      const VarLabel* mom_L_CCLabel;
      const VarLabel* dvdt_CCLabel;
      const VarLabel* cv_CCLabel;
      const VarLabel* temp_CCLabel;
      const VarLabel* speedSound_CCLabel;
      const VarLabel* rho_CCLabel;
      const VarLabel* rho_micro_CCLabel;
      const VarLabel* vol_frac_CCLabel;
      const VarLabel* mom_source_CCLabel;

      const VarLabel* press_NCLabel;

      const VarLabel* uvel_FCLabel;
      const VarLabel* vvel_FCLabel;
      const VarLabel* wvel_FCLabel;
      const VarLabel* uvel_FCMELabel;
      const VarLabel* vvel_FCMELabel;
      const VarLabel* wvel_FCMELabel;
    };

} // end namespace Uintah

#endif
