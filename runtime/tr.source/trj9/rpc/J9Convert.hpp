#include "j9.h"
#include "rpc/types.h"

namespace J9Convert
   {
   J9Class *from(const JAAS::IntoJ9Class &x)
      {
      switch (x.MessageKind_case())
         {
         case JAAS::IntoJ9Class::kJ9Class:
            return (J9Class *) x.j9_class();
         case JAAS::IntoJ9Class::kJ9Method:
            return J9_CLASS_FROM_METHOD((J9Method *) x.j9_method());
         default:;
         }
      return nullptr;
      }
   }
