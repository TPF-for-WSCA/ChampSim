import matplotlib.pyplot as plt
from matplotlib import colormaps
import numpy as np
import pandas as pd
import argparse
from matplotlib.colors import ListedColormap

from sklearn.datasets import make_circles, make_classification, make_moons
from sklearn.discriminant_analysis import QuadraticDiscriminantAnalysis
from sklearn.ensemble import AdaBoostClassifier, RandomForestClassifier
from sklearn.gaussian_process import GaussianProcessClassifier
from sklearn.gaussian_process.kernels import RBF
from sklearn.inspection import DecisionBoundaryDisplay
from sklearn.model_selection import train_test_split
from sklearn.naive_bayes import GaussianNB
from sklearn.neighbors import KNeighborsClassifier
from sklearn.neural_network import MLPClassifier
from sklearn.pipeline import make_pipeline
from sklearn.preprocessing import StandardScaler
from sklearn.svm import SVC
from sklearn.tree import DecisionTreeClassifier

names = [
    # "Nearest Neighbors",
    "Linear SVM",
    "RBF SVM",
    "RBF SVM",
    "RBF SVM",
    "RBF SVM",
    # "Gaussian Process",
    "Decision Tree",
    # "Random Forest",
    "Neural Net",
    "AdaBoost",
    "Naive Bayes",
    # "QDA",
]

classifiers = [
    # KNeighborsClassifier(3),
    SVC(kernel="linear", C=0.025, random_state=42),
    SVC(gamma=2, C=1, random_state=42),
    SVC(gamma=2, C=1, random_state=42),
    SVC(gamma=2, C=1, random_state=42),
    SVC(gamma=2, C=1, random_state=42),
    # GaussianProcessClassifier(1.0 * RBF(1.0), random_state=42),
    DecisionTreeClassifier(max_depth=5, random_state=42),
    # RandomForestClassifier(
    #     max_depth=5, n_estimators=10, max_features=64, random_state=42
    # ),
    MLPClassifier(alpha=1, max_iter=1000, random_state=42),
    AdaBoostClassifier(algorithm="SAMME", random_state=42),
    GaussianNB(),
    # QuadraticDiscriminantAnalysis(),
]


def main(args):
    data = pd.read_csv(args.file_path, sep="\t")

    X = []
    Y = []
    for [x, y] in data.values:
        X.append(
            [int(b) for b in ((int(x) >> 2) & 0x7FFFFFFFFFF).to_bytes(7, "little")]
        )
        Y.append(int(y))

    linearly_separable = (
        np.array([np.array(x) for x in X]),
        np.array([np.array(y) for y in Y]),
    )

    datasets = [
        linearly_separable,
    ]

    figure = plt.figure(figsize=(27, 9))
    i = 1
    # iterate over datasets
    for ds_cnt, ds in enumerate(datasets):
        # preprocess dataset, split into training and test part
        X, y = ds
        X_train, X_test, y_train, y_test = train_test_split(
            X, y, test_size=0.4, random_state=42
        )

        y_min, y_max = X[:, 1].min() - 0.5, X[:, 1].max() + 0.5
        x_min, x_max = X[:, 0].min() - 0.5, X[:, 0].max() + 0.5

        # just plot the dataset first
        cm = plt.cm.RdBu
        ax = plt.subplot(1, 1, i)
        if ds_cnt == 0:
            ax.set_title("Input data")
        # Plot the training points
        ax.scatter(
            X_train[:, 0],
            X_train[:, 1],
            c=y_train,
            edgecolors="k",
        )
        # Plot the testing points
        ax.scatter(
            X_test[:, 0],
            X_test[:, 1],
            c=y_test,
            alpha=0.6,
            edgecolors="k",
        )
        ax.set_xlim(x_min, x_max)
        ax.set_ylim(y_min, y_max)
        ax.set_xticks(())
        ax.set_yticks(())
        plt.tight_layout()
        # plt.show()

        # iterate over classifiers
        print("___________________________________________")
        print(f"{'| Predictor':<18}|{'Accuracy':>22} |")
        print("|-----------------|-----------------------|")
        for name, clf in zip(names, classifiers):

            clf = make_pipeline(StandardScaler(), clf)
            clf.fit(X_train, y_train)
            score = clf.score(X_test, y_test)
            print(f"|{name:<17}|{score:>22} |")
            # DecisionBoundaryDisplay.from_estimator(
            #     clf, X, cmap=cm, alpha=0.8, ax=ax, eps=0.5
            # )

        print("|=========================================|")


parser = argparse.ArgumentParser(prog="Classify offsets of PC")
parser.add_argument("file_path")

main(parser.parse_args())
