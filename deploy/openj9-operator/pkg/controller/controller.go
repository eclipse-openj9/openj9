package controller

import (
	prometheusv1 "github.com/coreos/prometheus-operator/pkg/apis/monitoring/v1"
	certmngrv1alpha2 "github.com/jetstack/cert-manager/pkg/apis/certmanager/v1alpha2"
	servingv1alpha1 "github.com/knative/serving/pkg/apis/serving/v1alpha1"
	imagev1 "github.com/openshift/api/image/v1"
	routev1 "github.com/openshift/api/route/v1"
	applicationsv1beta1 "sigs.k8s.io/application/pkg/apis/app/v1beta1"

	"sigs.k8s.io/controller-runtime/pkg/manager"
)

// AddToManagerFuncs is a list of functions to add all Controllers to the Manager
var AddToManagerFuncs []func(manager.Manager) error

// AddToManager adds all Controllers to the Manager
func AddToManager(m manager.Manager) error {
	if err := routev1.AddToScheme(m.GetScheme()); err != nil {
		return err
	}

	if err := servingv1alpha1.AddToScheme(m.GetScheme()); err != nil {
		return err
	}

	if err := prometheusv1.AddToScheme(m.GetScheme()); err != nil {
		return err
	}

	if err := certmngrv1alpha2.AddToScheme(m.GetScheme()); err != nil {
		return err
	}

	if err := imagev1.AddToScheme(m.GetScheme()); err != nil {
		return err
	}

	if err := applicationsv1beta1.AddToScheme(m.GetScheme()); err != nil {
		return err
	}

	for _, f := range AddToManagerFuncs {
		if err := f(m); err != nil {
			return err
		}
	}
	return nil
}
