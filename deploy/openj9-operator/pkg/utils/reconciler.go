package utils

import (
	"context"
	"fmt"
	"math"
	"strings"
	"time"

	certmngrv1alpha2 "github.com/jetstack/cert-manager/pkg/apis/certmanager/v1alpha2"
	v1 "github.com/jetstack/cert-manager/pkg/apis/meta/v1"
	routev1 "github.com/openshift/api/route/v1"
	"github.com/pkg/errors"
	openj9v1beta1 "github.com/eclipse/openj9/pkg/apis/openj9/v1beta1"
	"github.com/eclipse/openj9/pkg/common"
	corev1 "k8s.io/api/core/v1"
	apierrors "k8s.io/apimachinery/pkg/api/errors"
	kerrors "k8s.io/apimachinery/pkg/api/errors"
	metav1 "k8s.io/apimachinery/pkg/apis/meta/v1"
	"k8s.io/apimachinery/pkg/runtime"
	"k8s.io/apimachinery/pkg/runtime/schema"
	"k8s.io/apimachinery/pkg/types"
	"k8s.io/client-go/discovery"
	"k8s.io/client-go/rest"
	"k8s.io/client-go/tools/record"
	applicationsv1beta1 "sigs.k8s.io/application/pkg/apis/app/v1beta1"
	"sigs.k8s.io/controller-runtime/pkg/client"
	"sigs.k8s.io/controller-runtime/pkg/client/apiutil"
	"sigs.k8s.io/controller-runtime/pkg/controller"
	"sigs.k8s.io/controller-runtime/pkg/controller/controllerutil"
	"sigs.k8s.io/controller-runtime/pkg/reconcile"
	logf "sigs.k8s.io/controller-runtime/pkg/runtime/log"
)

// ReconcilerBase base reconciler with some common behaviour
type ReconcilerBase struct {
	client     client.Client
	scheme     *runtime.Scheme
	recorder   record.EventRecorder
	restConfig *rest.Config
	discovery  discovery.DiscoveryInterface
	controller controller.Controller
}

//NewReconcilerBase creates a new ReconcilerBase
func NewReconcilerBase(client client.Client, scheme *runtime.Scheme, restConfig *rest.Config, recorder record.EventRecorder) ReconcilerBase {
	return ReconcilerBase{
		client:     client,
		scheme:     scheme,
		recorder:   recorder,
		restConfig: restConfig,
	}
}

// GetController returns controller
func (r *ReconcilerBase) GetController() controller.Controller {
	return r.controller
}

// SetController sets controller
func (r *ReconcilerBase) SetController(c controller.Controller) {
	r.controller = c
}

// GetClient returns client
func (r *ReconcilerBase) GetClient() client.Client {
	return r.client
}

// GetRecorder returns the underlying recorder
func (r *ReconcilerBase) GetRecorder() record.EventRecorder {
	return r.recorder
}

// GetDiscoveryClient ...
func (r *ReconcilerBase) GetDiscoveryClient() (discovery.DiscoveryInterface, error) {
	if r.discovery == nil {
		var err error
		r.discovery, err = discovery.NewDiscoveryClientForConfig(r.restConfig)
		return r.discovery, err
	}

	return r.discovery, nil
}

// SetDiscoveryClient ...
func (r *ReconcilerBase) SetDiscoveryClient(discovery discovery.DiscoveryInterface) {
	r.discovery = discovery
}

var log = logf.Log.WithName("utils")

// CreateOrUpdate ...
func (r *ReconcilerBase) CreateOrUpdate(obj metav1.Object, owner metav1.Object, reconcile func() error) error {

	if owner != nil {
		controllerutil.SetControllerReference(owner, obj, r.scheme)
	}
	runtimeObj, ok := obj.(runtime.Object)
	if !ok {
		err := fmt.Errorf("%T is not a runtime.Object", obj)
		log.Error(err, "Failed to convert into runtime.Object")
		return err
	}
	result, err := controllerutil.CreateOrUpdate(context.TODO(), r.GetClient(), runtimeObj, reconcile)
	if err != nil {
		return err
	}

	var gvk schema.GroupVersionKind
	gvk, err = apiutil.GVKForObject(runtimeObj, r.scheme)
	if err == nil {
		log.Info("Reconciled", "Kind", gvk.Kind, "Namespace", obj.GetNamespace(), "Name", obj.GetName(), "Status", result)
	}

	return err
}

// DeleteResource deletes kubernetes resource
func (r *ReconcilerBase) DeleteResource(obj runtime.Object) error {
	err := r.client.Delete(context.TODO(), obj)
	if err != nil {
		if !apierrors.IsNotFound(err) {
			log.Error(err, "Unable to delete object ", "object", obj)
			return err
		}
		return nil
	}

	metaObj, ok := obj.(metav1.Object)
	if !ok {
		err := fmt.Errorf("%T is not a metav1.Object", obj)
		log.Error(err, "Failed to convert into metav1.Object")
		return err
	}

	var gvk schema.GroupVersionKind
	gvk, err = apiutil.GVKForObject(obj, r.scheme)
	if err == nil {
		log.Info("Reconciled", "Kind", gvk.Kind, "Name", metaObj.GetName(), "Status", "deleted")
	}
	return nil
}

// DeleteResources ...
func (r *ReconcilerBase) DeleteResources(resources []runtime.Object) error {
	for i := range resources {
		err := r.DeleteResource(resources[i])
		if err != nil {
			return err
		}
	}
	return nil
}

// GetOpConfigMap ...
func (r *ReconcilerBase) GetOpConfigMap(name string, ns string) (*corev1.ConfigMap, error) {
	configMap := &corev1.ConfigMap{}
	err := r.GetClient().Get(context.TODO(), types.NamespacedName{Name: name, Namespace: ns}, configMap)
	if err != nil {
		return nil, err
	}
	return configMap, nil
}

// ManageError ...
func (r *ReconcilerBase) ManageError(issue error, conditionType common.StatusConditionType, ba common.BaseComponent) (reconcile.Result, error) {
	s := ba.GetStatus()
	rObj := ba.(runtime.Object)
	mObj := ba.(metav1.Object)
	logger := log.WithValues("ba.Namespace", mObj.GetNamespace(), "ba.Name", mObj.GetName())
	logger.Error(issue, "ManageError", "Condition", conditionType, "ba", ba)
	r.GetRecorder().Event(rObj, "Warning", "ProcessingError", issue.Error())

	oldCondition := s.GetCondition(conditionType)
	if oldCondition == nil {
		oldCondition = &openj9v1beta1.StatusCondition{LastUpdateTime: metav1.Time{}}
	}

	lastUpdate := oldCondition.GetLastUpdateTime().Time
	lastStatus := oldCondition.GetStatus()

	// Keep the old `LastTransitionTime` when status has not changed
	nowTime := metav1.Now()
	transitionTime := oldCondition.GetLastTransitionTime()
	if lastStatus == corev1.ConditionTrue {
		transitionTime = &nowTime
	}

	newCondition := s.NewCondition()
	newCondition.SetLastTransitionTime(transitionTime)
	newCondition.SetLastUpdateTime(nowTime)
	newCondition.SetReason(string(apierrors.ReasonForError(issue)))
	newCondition.SetType(conditionType)
	newCondition.SetMessage(issue.Error())
	newCondition.SetStatus(corev1.ConditionFalse)

	s.SetCondition(newCondition)

	err := r.UpdateStatus(rObj)
	if err != nil {

		if apierrors.IsConflict(issue) {
			return reconcile.Result{Requeue: true}, nil
		}
		logger.Error(err, "Unable to update status")
		return reconcile.Result{
			RequeueAfter: time.Second,
			Requeue:      true,
		}, nil
	}

	// StatusReasonInvalid means the requested create or update operation cannot be
	// completed due to invalid data provided as part of the request. Don't retry.
	if apierrors.IsInvalid(issue) {
		return reconcile.Result{}, nil
	}

	var retryInterval time.Duration
	if lastUpdate.IsZero() || lastStatus == corev1.ConditionTrue {
		retryInterval = time.Second
	} else {
		retryInterval = newCondition.GetLastUpdateTime().Sub(lastUpdate).Round(time.Second)
	}

	return reconcile.Result{
		RequeueAfter: time.Duration(math.Min(float64(retryInterval.Nanoseconds()*2), float64(time.Hour.Nanoseconds()*6))),
		Requeue:      true,
	}, nil
}

// ManageSuccess ...
func (r *ReconcilerBase) ManageSuccess(conditionType common.StatusConditionType, ba common.BaseComponent) (reconcile.Result, error) {
	s := ba.GetStatus()
	oldCondition := s.GetCondition(conditionType)
	if oldCondition == nil {
		oldCondition = &openj9v1beta1.StatusCondition{LastUpdateTime: metav1.Time{}}
	}

	// Keep the old `LastTransitionTime` when status has not changed
	nowTime := metav1.Now()
	transitionTime := oldCondition.GetLastTransitionTime()
	if oldCondition.GetStatus() == corev1.ConditionFalse {
		transitionTime = &nowTime
	}

	statusCondition := s.NewCondition()
	statusCondition.SetLastTransitionTime(transitionTime)
	statusCondition.SetLastUpdateTime(nowTime)
	statusCondition.SetReason("")
	statusCondition.SetMessage("")
	statusCondition.SetStatus(corev1.ConditionTrue)
	statusCondition.SetType(conditionType)

	s.SetCondition(statusCondition)
	err := r.UpdateStatus(ba.(runtime.Object))
	if err != nil {
		log.Error(err, "Unable to update status")
		return reconcile.Result{
			RequeueAfter: time.Second,
			Requeue:      true,
		}, nil
	}
	return reconcile.Result{}, nil
}

// IsGroupVersionSupported ...
func (r *ReconcilerBase) IsGroupVersionSupported(groupVersion string, kind string) (bool, error) {
	cli, err := r.GetDiscoveryClient()
	if err != nil {
		log.Error(err, "Failed to return a discovery client for the current reconciler")
		return false, err
	}

	res, err := cli.ServerResourcesForGroupVersion(groupVersion)
	if err != nil {
		if apierrors.IsNotFound(err) {
			return false, nil
		}

		return false, err
	}

	for _, v := range res.APIResources {
		if v.Kind == kind {
			return true, nil
		}
	}

	return false, nil
}

// UpdateStatus updates the fields corresponding to the status subresource for the object
func (r *ReconcilerBase) UpdateStatus(obj runtime.Object) error {
	return r.GetClient().Status().Update(context.Background(), obj)
}

// ReconcileCertificate used to manage cert-manager integration
func (r *ReconcilerBase) ReconcileCertificate(ba common.BaseComponent) (reconcile.Result, error) {
	owner := ba.(metav1.Object)
	if ok, err := r.IsGroupVersionSupported(certmngrv1alpha2.SchemeGroupVersion.String(), "Certificate"); err != nil {
		r.ManageError(err, common.StatusConditionTypeReconciled, ba)
	} else if ok {
		if ba.GetService() != nil && ba.GetService().GetCertificate() != nil {
			crt := &certmngrv1alpha2.Certificate{ObjectMeta: metav1.ObjectMeta{Name: owner.GetName() + "-svc-crt", Namespace: owner.GetNamespace()}}
			err = r.CreateOrUpdate(crt, owner, func() error {
				obj := ba.(metav1.Object)
				crt.Labels = ba.GetLabels()
				crt.Annotations = MergeMaps(crt.Annotations, ba.GetAnnotations())
				crt.Spec = ba.GetService().GetCertificate().GetSpec()
				if crt.Spec.Duration == nil {
					crt.Spec.Duration = &metav1.Duration{Duration: time.Hour * 24 * 365}
				}
				if crt.Spec.RenewBefore == nil {
					crt.Spec.RenewBefore = &metav1.Duration{Duration: time.Hour * 24 * 31}
				}
				crt.Spec.CommonName = obj.GetName() + "." + obj.GetNamespace() + "." + "svc"
				if crt.Spec.SecretName == "" {
					crt.Spec.SecretName = obj.GetName() + "-svc-tls"
				}
				if len(crt.Spec.DNSNames) == 0 {
					crt.Spec.DNSNames = append(crt.Spec.DNSNames, crt.Spec.CommonName)
				}
				return nil
			})
			if err != nil {
				return r.ManageError(err, common.StatusConditionTypeReconciled, ba)
			}
			crtReady := false
			for i := range crt.Status.Conditions {
				if crt.Status.Conditions[i].Type == certmngrv1alpha2.CertificateConditionReady {
					if crt.Status.Conditions[i].Status == v1.ConditionTrue {
						crtReady = true
					}
				}
			}
			if !crtReady {
				c := ba.GetStatus().NewCondition()
				c.SetType(common.StatusConditionTypeReconciled)
				c.SetStatus(corev1.ConditionFalse)
				c.SetReason("CertificateNotReady")
				c.SetMessage("Waiting for service certificate to be generated")
				ba.GetStatus().SetCondition(c)
				rtObj := ba.(runtime.Object)
				r.UpdateStatus(rtObj)
				return reconcile.Result{}, errors.New("Certificate not ready")
			}

		} else {
			crt := &certmngrv1alpha2.Certificate{ObjectMeta: metav1.ObjectMeta{Name: owner.GetName() + "-svc-crt", Namespace: owner.GetNamespace()}}
			err = r.DeleteResource(crt)
			if err != nil {
				return r.ManageError(err, common.StatusConditionTypeReconciled, ba)
			}
		}

		if ba.GetExpose() != nil && *ba.GetExpose() && ba.GetRoute() != nil && ba.GetRoute().GetCertificate() != nil {
			crt := &certmngrv1alpha2.Certificate{ObjectMeta: metav1.ObjectMeta{Name: owner.GetName() + "-route-crt", Namespace: owner.GetNamespace()}}
			err = r.CreateOrUpdate(crt, owner, func() error {
				obj := ba.(metav1.Object)
				crt.Labels = ba.GetLabels()
				crt.Annotations = MergeMaps(crt.Annotations, ba.GetAnnotations())
				crt.Spec = ba.GetRoute().GetCertificate().GetSpec()
				if crt.Spec.Duration == nil {
					crt.Spec.Duration = &metav1.Duration{Duration: time.Hour * 24 * 365}
				}
				if crt.Spec.RenewBefore == nil {
					crt.Spec.RenewBefore = &metav1.Duration{Duration: time.Hour * 24 * 31}
				}
				if crt.Spec.SecretName == "" {
					crt.Spec.SecretName = obj.GetName() + "-route-tls"
				}
				// use routes host if no DNS information provided on certificate
				if crt.Spec.CommonName == "" {
					crt.Spec.CommonName = ba.GetRoute().GetHost()
					if crt.Spec.CommonName == "" && common.Config[common.OpConfigDefaultHostname] != "" {
						crt.Spec.CommonName = obj.GetName() + "-" + obj.GetNamespace() + "." + common.Config[common.OpConfigDefaultHostname]
					}
				}
				if len(crt.Spec.DNSNames) == 0 {
					crt.Spec.DNSNames = append(crt.Spec.DNSNames, crt.Spec.CommonName)
				}
				return nil
			})
			if err != nil {
				return r.ManageError(err, common.StatusConditionTypeReconciled, ba)
			}
			crtReady := false
			for i := range crt.Status.Conditions {
				if crt.Status.Conditions[i].Type == certmngrv1alpha2.CertificateConditionReady {
					if crt.Status.Conditions[i].Status == v1.ConditionTrue {
						crtReady = true
					}
				}
			}
			if !crtReady {
				log.Info("Status", "Conditions", crt.Status.Conditions)
				c := ba.GetStatus().NewCondition()
				c.SetType(common.StatusConditionTypeReconciled)
				c.SetStatus(corev1.ConditionFalse)
				c.SetReason("CertificateNotReady")
				c.SetMessage("Waiting for route certificate to be generated")
				ba.GetStatus().SetCondition(c)
				rtObj := ba.(runtime.Object)
				r.UpdateStatus(rtObj)
				return reconcile.Result{}, errors.New("Certificate not ready")
			}
		} else {
			crt := &certmngrv1alpha2.Certificate{ObjectMeta: metav1.ObjectMeta{Name: owner.GetName() + "-route-crt", Namespace: owner.GetNamespace()}}
			err = r.DeleteResource(crt)
			if err != nil {
				return r.ManageError(err, common.StatusConditionTypeReconciled, ba)
			}
		}

	}
	return reconcile.Result{}, nil
}

// IsOpenShift returns true if the operator is running on an OpenShift platform
func (r *ReconcilerBase) IsOpenShift() bool {
	isOpenShift, err := r.IsGroupVersionSupported(routev1.SchemeGroupVersion.String(), "Route")
	if err != nil {
		return false
	}
	return isOpenShift
}

// IsApplicationSupported checks if Application
func (r *ReconcilerBase) IsApplicationSupported() bool {
	isApplicationSupported, err := r.IsGroupVersionSupported(applicationsv1beta1.SchemeGroupVersion.String(), "Application")
	if err != nil {
		return false
	}
	return isApplicationSupported
}

// GetRouteTLSValues returns certificate an key values to be used in the route
func (r *ReconcilerBase) GetRouteTLSValues(ba common.BaseComponent) (key string, cert string, ca string, destCa string, err error) {
	key, cert, ca, destCa = "", "", "", ""
	mObj := ba.(metav1.Object)
	if ba.GetService() != nil && (ba.GetService().GetCertificate() != nil || ba.GetService().GetCertificateSecretRef() != nil) {
		tlsSecret := &corev1.Secret{}
		secretName := mObj.GetName() + "-svc-tls"
		if ba.GetService().GetCertificate() != nil && ba.GetService().GetCertificate().GetSpec().SecretName != "" {
			secretName = ba.GetService().GetCertificate().GetSpec().SecretName
		}
		if ba.GetService().GetCertificateSecretRef() != nil {
			secretName = *ba.GetService().GetCertificateSecretRef()
		}
		err := r.GetClient().Get(context.TODO(), types.NamespacedName{Name: secretName, Namespace: mObj.GetNamespace()}, tlsSecret)
		if err != nil {
			r.ManageError(err, common.StatusConditionTypeReconciled, ba)
			return "", "", "", "", err
		}
		caCrt, ok := tlsSecret.Data["ca.crt"]
		if ok {
			destCa = string(caCrt)
		}
	}
	if ba.GetRoute() != nil && (ba.GetRoute().GetCertificate() != nil || ba.GetRoute().GetCertificateSecretRef() != nil) {
		tlsSecret := &corev1.Secret{}
		secretName := mObj.GetName() + "-route-tls"
		if ba.GetRoute().GetCertificate() != nil && ba.GetRoute().GetCertificate().GetSpec().SecretName != "" {
			secretName = ba.GetRoute().GetCertificate().GetSpec().SecretName
		}
		if ba.GetRoute().GetCertificateSecretRef() != nil {
			secretName = *ba.GetRoute().GetCertificateSecretRef()
		}
		err := r.GetClient().Get(context.TODO(), types.NamespacedName{Name: secretName, Namespace: mObj.GetNamespace()}, tlsSecret)
		if err != nil {
			r.ManageError(err, common.StatusConditionTypeReconciled, ba)
			return "", "", "", "", err
		}
		v, ok := tlsSecret.Data["ca.crt"]
		if ok {
			ca = string(v)
		}
		v, ok = tlsSecret.Data["tls.crt"]
		if ok {
			cert = string(v)
		}
		v, ok = tlsSecret.Data["tls.key"]
		if ok {
			key = string(v)
		}
		v, ok = tlsSecret.Data["destCA.crt"]
		if ok {
			destCa = string(v)
		}
	}
	return key, cert, ca, destCa, nil
}

// GetSelectorLabelsFromApplications finds application CRs with the specified name in the BaseComponent's namespace and returns labels in `selector.matchLabels`.
// If it fails to find in the current namespace, it looks up in the whole cluster and aggregates all labels in `selector.matchLabels`.
func (r *ReconcilerBase) GetSelectorLabelsFromApplications(ba common.BaseComponent) (map[string]string, error) {
	mObj := ba.(metav1.Object)
	allSelectorLabels := map[string]string{}
	app := &applicationsv1beta1.Application{}
	key := types.NamespacedName{Name: ba.GetApplicationName(), Namespace: mObj.GetNamespace()}
	var err error
	if err = r.GetClient().Get(context.Background(), key, app); err == nil {
		if app.Spec.Selector != nil {
			for name, value := range app.Spec.Selector.MatchLabels {
				allSelectorLabels[name] = value
			}
		}
	} else if err != nil && kerrors.IsNotFound(err) {
		apps := &applicationsv1beta1.ApplicationList{}
		if err = r.GetClient().List(context.Background(), apps, client.InNamespace("")); err == nil {
			for _, app := range apps.Items {
				if app.Name == ba.GetApplicationName() && app.Annotations != nil {
					namespaces := strings.Split(app.Annotations["kappnav.component.namespaces"], ",")
					for i := range namespaces {
						namespaces[i] = strings.TrimSpace(namespaces[i])
					}
					if ContainsString(namespaces, mObj.GetNamespace()) && app.Spec.Selector != nil {
						for name, value := range app.Spec.Selector.MatchLabels {
							allSelectorLabels[name] = value
						}
					}
				}
			}
		}
	}
	if err != nil && !kerrors.IsNotFound(err) {
		return nil, err
	}
	return allSelectorLabels, nil
}
