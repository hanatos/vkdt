import numpy as np
from pysr import PySRRegressor

model = PySRRegressor(
    maxsize=120,
    niterations=10000,  # < Increase me for better results
    binary_operators=["+", "*"],
    unary_operators=[
# give it enough to reproduce sigmoidal spectra:
#       "sqrt",
        "cos",
#         "exp",
#         "sin",
        "inv(x) = 1/x",
        # ^ Custom operator (julia syntax)
    ],
    extra_sympy_mappings={"inv": lambda x: 1 / x},
    # ^ Define operator for SymPy as well
    elementwise_loss="loss(prediction, target) = (prediction - target)^2",
    # ^ Custom loss function (julia syntax)
    batching=True,
    batch_size=2000,
)
# model = PySRRegressor.from_file(run_directory='outputs/20260514_093830_dsT3QG')
# model.set_params(extra_sympy_mappings={'inv': lambda x: 1/x})
# model.elementwise_loss="loss(prediction, target) = (prediction - target)^2"
# model.batching = True
# model.batch_size = 2000
# model.iterations=20000
# model.warm_start = True

# x: wavelength, cr, cb (cr = r/(r+g+b), cb = b/(r+g+b))
# y: normalised spectral response per wavelength

x = np.loadtxt('sr-x.dat', delimiter=' ',dtype=np.float64)
y = np.loadtxt('sr-y.dat', delimiter=' ',dtype=np.float64)
print(x)
print(y)
model.fit(x, y)

print(model)

# model.predict(x)
# model.predict(x, 3) # for third equation
