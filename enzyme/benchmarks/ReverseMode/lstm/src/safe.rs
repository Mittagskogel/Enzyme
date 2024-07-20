use std::slice;

// Sigmoid on scalar
fn sigmoid(x: f64) -> f64 {
    1.0 / (1.0 + (-x).exp())
}

// log(sum(exp(x), 2))
#[inline]
fn logsumexp(vect: &[f64]) -> f64 {
    let mut sum = 0.0;
    for &val in vect {
        sum += val.exp();
    }
    sum += 2.0; // Adding 2 to sum
    sum.ln()
}

// LSTM OBJECTIVE
// The LSTM model
fn lstm_model(
    hsize: usize,
    weight: &[f64],
    bias: &[f64],
    hidden: &mut [f64],
    cell: &mut [f64],
    input: &[f64],
) {
    let mut gates = vec![0.0; 4 * hsize];
    let gates = &mut gates[..4 * hsize];
    let (a, b) = gates.split_at_mut(2 * hsize);
    let (forget, ingate) = a.split_at_mut(hsize);
    let (outgate, change) = b.split_at_mut(hsize);

    assert_eq!(weight.len(), 4 * hsize);
    assert_eq!(bias.len(), 4 * hsize);
    assert_eq!(hidden.len(), hsize);
    assert_eq!(ingate.len(), hsize);
    assert_eq!(change.len(), hsize);
    assert!(cell.len() >= hsize);
    assert!(input.len() >= hsize);
    // Using unchecked indexing here was slightly slower for some reason
    for i in 0..hsize {
        forget[i] = sigmoid(input[i] * weight[i] + bias[i]);
        ingate[i] = sigmoid(hidden[i] * weight[hsize + i] + bias[hsize + i]);
        outgate[i] = sigmoid(input[i] * weight[2 * hsize + i] + bias[2 * hsize + i]);
        change[i] = (hidden[i] * weight[3 * hsize + i] + bias[3 * hsize + i]).tanh();
    }

    // caching cell
    for i in 0..hsize {
        cell[i] = cell[i] * forget[i] + ingate[i] * change[i];
    }

    for i in 0..hsize {
        hidden[i] = outgate[i] * cell[i].tanh();
    }
}

// Predict LSTM output given an input
fn lstm_predict(
    l: usize,
    b: usize,
    w: &[f64],
    w2: &[f64],
    s: &mut [f64],
    x: &[f64],
    x2: &mut [f64],
) {
    for i in 0..b {
        x2[i] = x[i] * w2[i];
    }
    
    let (s1, s2) = s.split_at_mut(b);
    lstm_model(
        b,
        &w[0..b * 4],
        &w[b * 4..2 * b * 4],
        s1,
        s2,
        x2.as_mut(),
    );

    assert_eq!(s.len(), 2 * b * l);
    assert_eq!(w.len(), 4 * b * l);
    for i in 1..l {
        let i = i * 2 * b;
        let (xp, s1, s2) = {
            let tmp = &mut s[i - 2 * b..];
            let (a, d) = tmp.split_at_mut(2 * b);
            let (d, c) = d.split_at_mut(b);
            (a, d, c)
        };
        let (w1, w2) = w.split_at((i + b) * 4);

        lstm_model(
            b,
            //&w1[i * 4..],
            //&w2[0..(i + 2 * b) * 4],
            &w[i * 4..(i + b) * 4],
            &w[(i + b) * 4..(i + 2 * b) * 4],
            s1,
            s2,
            xp,
        );
    }

    let i = 2 * l * b;
    let xp = &s[i - 2 * b..];

    for i in 0..b {
        x2[i] = xp[i] * w2[b + i] + w2[2 * b + i];
    }
}

// LSTM objective (loss function)
#[autodiff(
    d_lstm_objective,
    Reverse,
    Const,
    Const,
    Const,
    Duplicated,
    Duplicated,
    Const,
    Const,
    Duplicated
)]
pub(crate) fn lstm_objective(
    l: usize,
    c: usize,
    b: usize,
    main_params: &[f64],
    extra_params: &[f64],
    state: &mut [f64],
    sequence: &[f64],
    loss: &mut f64,
) {
    let mut total = 0.0;

    let mut input = &sequence[..b];
    let mut ypred = vec![0.0; b];
    let mut ynorm = vec![0.0; b];

    assert!(b > 0);

    let limit = (c - 1) * b;
    for j in 0..(c - 1) {
        let t = j * b;
        lstm_predict(l, b, main_params, extra_params, state, input, &mut ypred);
        let lse = logsumexp(&ypred);
        for i in 0..b {
            ynorm[i] = ypred[i] - lse;
        }

        let ygold = &sequence[t + b..];
        for i in 0..b {
            total += ygold[i] * ynorm[i];
        }

        input = ygold;
    }
    let count = (c - 1) * b;

    *loss = -total / count as f64;
}

#[no_mangle]
pub extern "C" fn rust_lstm_objective(
    l: usize,
    c: usize,
    b: usize,
    main_params: *const f64,
    extra_params: *const f64,
    state: *mut f64,
    sequence: *const f64,
    loss: *mut f64,
) {
    let (main_params, extra_params, state, sequence) = unsafe {
        (
            slice::from_raw_parts(main_params, 2 * l * 4 * b),
            slice::from_raw_parts(extra_params, 3 * b),
            slice::from_raw_parts_mut(state, 2 * l * b),
            slice::from_raw_parts(sequence, c * b),
        )
    };

    unsafe {
        lstm_objective(
            l,
            c,
            b,
            main_params,
            extra_params,
            state,
            sequence,
            &mut *loss,
        );
    }
}

#[no_mangle]
pub extern "C" fn rust_dlstm_objective(
    l: usize,
    c: usize,
    b: usize,
    main_params: *const f64,
    d_main_params: *mut f64,
    extra_params: *const f64,
    d_extra_params: *mut f64,
    state: *mut f64,
    sequence: *const f64,
    res: *mut f64,
    d_res: *mut f64,
) {
    let (main_params, d_main_params, extra_params, d_extra_params, state, sequence) = unsafe {
        (
            slice::from_raw_parts(main_params, 2 * l * 4 * b),
            slice::from_raw_parts_mut(d_main_params, 2 * l * 4 * b),
            slice::from_raw_parts(extra_params, 3 * b),
            slice::from_raw_parts_mut(d_extra_params, 3 * b),
            slice::from_raw_parts_mut(state, 2 * l * b),
            slice::from_raw_parts(sequence, c * b),
        )
    };

    unsafe {
        d_lstm_objective(
            l,
            c,
            b,
            main_params,
            d_main_params,
            extra_params,
            d_extra_params,
            state,
            sequence,
            &mut *res,
            &mut *d_res,
        );
    }
}