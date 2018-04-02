inlineable // FIXME(sil-serialize-all)
public func sequence<T>(first: T, next: @escaping (T) -> T?) -> UnfoldFirstSequence<T> {
    // The trivial implementation where the state is the next value to return
    // has the downside of being unnecessarily eager (it evaluates `next` one
    // step in advance). We solve this by using a boolean value to disambiguate
    // between the first value (that's computed in advance) and the rest.
    return sequence(state: (first, true), next: { (state: inout (T?, Bool)) -> T? in
        switch state {
        case (let value, true):
            state.1 = false
            return value
        case (let value?, _):
            let nextValue = next(value)
            state.0 = nextValue
            return nextValue
        case (nil, _):
            return nil
        }
    })
}

