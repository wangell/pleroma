// Allogenes

#include <iostream>
#include "../shared_src/protoloma.pb.h"

int main(int argc, char** argv) {
  romabuf::PleromaMessage message;

  message.set_vat_id(0);
  message.set_entity_id(1);
  message.set_function_id(1);

  std::cout << message.DebugString() << std::endl;
  std::cout << message.DebugString() << std::endl;

}
