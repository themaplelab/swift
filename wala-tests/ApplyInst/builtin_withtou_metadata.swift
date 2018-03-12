public func transcode<Foo: Unicode.Encoding>(_ a: Foo) -> Bool {
  return Foo.self == UTF16.self
}
