// d=13
// p=ceil(log_2(13))=4
// m=5286113596 = 0x13b13b13c
uint32_t div13(uint32_t n) {
  return (static_cast<uint64_t>(n) * 5286113596ul) >> 36;
}
