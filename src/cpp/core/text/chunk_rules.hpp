#ifndef PIPER_CORE_TEXT_CHUNK_RULES_HPP_
#define PIPER_CORE_TEXT_CHUNK_RULES_HPP_

#include <cstddef>
#include <string>

namespace piper::text {

std::size_t chooseSmartSplitPoint(const std::string &text, std::size_t offset,
                                  std::size_t preferredBytes);

} // namespace piper::text

#endif // PIPER_CORE_TEXT_CHUNK_RULES_HPP_
