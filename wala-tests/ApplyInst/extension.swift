extension Comparable {
  @_inlineable
  public static func <= (lhs: Self, rhs: Self) -> Bool {
    return !(rhs < lhs)
  }
}
