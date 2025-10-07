package utils

import (
	"context"
	"encoding/json"
	"fmt"
	"reflect"
	"sort"
	"strings"

	"github.com/pkg/errors"
	"github.com/eclipse/openj9/pkg/common"
	corev1 "k8s.io/api/core/v1"
	kerrors "k8s.io/apimachinery/pkg/api/errors"
	metav1 "k8s.io/apimachinery/pkg/apis/meta/v1"
	"k8s.io/apimachinery/pkg/apis/meta/v1/unstructured"
	"k8s.io/apimachinery/pkg/runtime"
	"k8s.io/apimachinery/pkg/runtime/schema"
	"k8s.io/apimachinery/pkg/types"
	"sigs.k8s.io/controller-runtime/pkg/client"
	"sigs.k8s.io/controller-runtime/pkg/client/apiutil"
	"sigs.k8s.io/controller-runtime/pkg/controller/controllerutil"
	"sigs.k8s.io/controller-runtime/pkg/handler"
	"sigs.k8s.io/controller-runtime/pkg/reconcile"
	"sigs.k8s.io/controller-runtime/pkg/source"
)

// String constants
const (
	APIVersion = "apiVersion"
	Kind       = "kind"
	Metadata   = "metadata"
	Spec       = "spec"
)

// SyncSecretAcrossNamespace syncs up the secret data across a namespace
func (r *ReconcilerBase) SyncSecretAcrossNamespace(fromSecret *corev1.Secret, namespace string) error {
	toSecret := &corev1.Secret{}
	err := r.client.Get(context.TODO(), types.NamespacedName{Name: fromSecret.Name, Namespace: namespace}, toSecret)
	if err != nil {
		return err
	}
	toSecret.Data = fromSecret.Data
	return r.client.Update(context.TODO(), toSecret)
}

// AsOwner returns an owner reference object based on the input object. Also can set controller field on the owner ref.
func (r *ReconcilerBase) AsOwner(rObj runtime.Object, controller bool) (metav1.OwnerReference, error) {
	mObj, ok := rObj.(metav1.Object)
	if !ok {
		err := errors.Errorf("%T is not a metav1.Object", rObj)
		log.Error(err, "failed to convert into metav1.Object")
		return metav1.OwnerReference{}, err
	}

	gvk, err := apiutil.GVKForObject(rObj, r.scheme)
	if err != nil {
		log.Error(err, "failed to get GroupVersionKind associated with the runtime.Object", mObj)
		return metav1.OwnerReference{}, err
	}

	return metav1.OwnerReference{
		APIVersion: gvk.Group + "/" + gvk.Version,
		Kind:       gvk.Kind,
		Name:       mObj.GetName(),
		UID:        mObj.GetUID(),
		Controller: &controller,
	}, nil
}

// GetServiceBindingCreds returns a map containing username/password string values based on 'cr.spec.service.provides.auth'
func (r *ReconcilerBase) GetServiceBindingCreds(ba common.BaseComponent) (map[string]string, error) {
	if ba.GetService() == nil || ba.GetService().GetProvides() == nil || ba.GetService().GetProvides().GetAuth() == nil {
		return nil, errors.Errorf("auth is not set on the object %s", ba)
	}
	metaObj := ba.(metav1.Object)
	authMap := map[string]string{}

	auth := ba.GetService().GetProvides().GetAuth()
	getCred := func(key string, getCredF func() corev1.SecretKeySelector) error {
		if getCredF() != (corev1.SecretKeySelector{}) {
			cred, err := getCredFromSecret(metaObj.GetNamespace(), getCredF(), key, r.client)
			if err != nil {
				return err
			}
			authMap[key] = cred
		}
		return nil
	}
	err := getCred("username", auth.GetUsername)
	err = getCred("password", auth.GetPassword)
	if err != nil {
		return nil, err
	}
	return authMap, nil
}

func getCredFromSecret(namespace string, sel corev1.SecretKeySelector, cred string, client client.Client) (string, error) {
	secret := &corev1.Secret{}
	err := client.Get(context.TODO(), types.NamespacedName{Name: sel.Name, Namespace: namespace}, secret)
	if err != nil {
		return "", errors.Wrapf(err, "unable to fetch credential %q from secret %q", cred, sel.Name)
	}

	if s, ok := secret.Data[sel.Key]; ok {
		return string(s), nil
	}
	return "", errors.Errorf("unable to find credential %q in secret %q using key %q", cred, sel.Name, sel.Key)
}

// ReconcileProvides ...
func (r *ReconcilerBase) ReconcileProvides(ba common.BaseComponent) (_ reconcile.Result, err error) {
	mObj := ba.(metav1.Object)
	logger := log.WithValues("ba.Namespace", mObj.GetNamespace(), "ba.Name", mObj.GetName())

	secretName := BuildServiceBindingSecretName(mObj.GetName(), mObj.GetNamespace())
	provides := ba.GetService().GetProvides()
	if provides != nil && provides.GetCategory() == common.ServiceBindingCategoryOpenAPI {
		var creds map[string]string
		if provides.GetAuth() != nil {
			if creds, err = r.GetServiceBindingCreds(ba); err != nil {
				r.requeueError(ba, errors.Wrapf(err, "service binding dependency not satisfied"))
			}
		}

		secretMeta := metav1.ObjectMeta{
			Name:      secretName,
			Namespace: mObj.GetNamespace(),
		}
		providerSecret := &corev1.Secret{ObjectMeta: secretMeta}
		err = r.CreateOrUpdate(providerSecret, mObj, func() error {
			CustomizeServiceBindingSecret(providerSecret, creds, ba)
			return nil
		})
		if err != nil {
			logger.Error(err, "Failed to reconcile provider secret")
			return r.ManageError(err, common.StatusConditionTypeReconciled, ba)
		}

		r.done(ba)
	} else {
		providerSecret := &corev1.Secret{}
		err = r.GetClient().Get(context.TODO(), types.NamespacedName{Name: secretName, Namespace: mObj.GetNamespace()}, providerSecret)
		if err != nil {
			if kerrors.IsNotFound(err) {
				logger.V(4).Info(fmt.Sprintf("Unable to find secret %q in namespace %q", secretName, mObj.GetNamespace()))
			} else {
				return r.ManageError(err, common.StatusConditionTypeReconciled, ba)
			}
		} else {
			// Delete all copies of this secret in other namespaces
			copiedToNamespacesKey := getCopiedToNamespacesAnnotationKey(ba)
			if providerSecret.Annotations[copiedToNamespacesKey] != "" {
				namespaces := strings.Split(providerSecret.Annotations[copiedToNamespacesKey], ",")
				for _, ns := range namespaces {
					err = r.GetClient().Delete(context.TODO(), &corev1.Secret{ObjectMeta: metav1.ObjectMeta{Name: secretName, Namespace: ns}})
					if err != nil {
						if kerrors.IsNotFound(err) {
							logger.V(4).Info(fmt.Sprintf("unable to find secret %q in namespace %q", secretName, mObj.GetNamespace()))
						} else {
							return r.ManageError(err, common.StatusConditionTypeReconciled, ba)
						}
					}
				}
			}

			// Delete the secret itself
			err = r.GetClient().Delete(context.TODO(), &corev1.Secret{ObjectMeta: metav1.ObjectMeta{Name: secretName, Namespace: mObj.GetNamespace()}})
			if err != nil {
				return r.ManageError(err, common.StatusConditionTypeReconciled, ba)
			}
		}
	}

	return r.done(ba)
}

// ReconcileConsumes ...
func (r *ReconcilerBase) ReconcileConsumes(ba common.BaseComponent) (reconcile.Result, error) {
	rObj := ba.(runtime.Object)
	mObj := ba.(metav1.Object)
	for _, con := range ba.GetService().GetConsumes() {
		if con.GetCategory() == common.ServiceBindingCategoryOpenAPI {
			conNamespace := con.GetNamespace()
			if conNamespace == "" {
				conNamespace = mObj.GetNamespace()
			}
			secretName := BuildServiceBindingSecretName(con.GetName(), conNamespace)
			existingSecret := &corev1.Secret{}
			err := r.GetClient().Get(context.TODO(), types.NamespacedName{Name: secretName, Namespace: conNamespace}, existingSecret)
			if err != nil {
				if kerrors.IsNotFound(err) {
					delErr := r.DeleteResource(&corev1.Secret{ObjectMeta: metav1.ObjectMeta{Name: secretName, Namespace: mObj.GetNamespace()}})
					if delErr != nil && !kerrors.IsNotFound(delErr) {
						delErr = errors.Wrapf(delErr, "unable to delete orphaned secret %q from namespace %q", secretName, mObj.GetNamespace())
						err = errors.Wrapf(delErr, "unable to find service binding secret %q for service %q in namespace %q", secretName, con.GetName(), conNamespace)
					} else {
						err = errors.Wrapf(err, "unable to find service binding secret %q for service %q in namespace %q", secretName, con.GetName(), conNamespace)
					}
				}
				return r.requeueError(ba, errors.Wrapf(err, "service binding dependency not satisfied"))
			}

			if existingSecret.Annotations == nil {
				existingSecret.Annotations = map[string]string{}
			}
			copiedToNamespacesKey := getCopiedToNamespacesAnnotationKey(ba)
			existingSecret.Annotations[copiedToNamespacesKey] = AppendIfNotSubstring(mObj.GetNamespace(), existingSecret.Annotations[copiedToNamespacesKey])
			err = r.GetClient().Update(context.TODO(), existingSecret)
			if err != nil {
				r.requeueError(ba, errors.Wrapf(err, "failed to update service provider secret"))
			}

			copiedSecret := &corev1.Secret{}
			err = r.GetClient().Get(context.TODO(), types.NamespacedName{Name: secretName, Namespace: mObj.GetNamespace()}, copiedSecret)
			consumedByKey := getConsumedByAnnotationKey(ba)
			if kerrors.IsNotFound(err) {
				owner, _ := r.AsOwner(rObj, false)
				copiedSecret = &corev1.Secret{
					ObjectMeta: metav1.ObjectMeta{
						Name:            secretName,
						Namespace:       mObj.GetNamespace(),
						Labels:          existingSecret.Labels,
						OwnerReferences: []metav1.OwnerReference{owner},
						Annotations:     map[string]string{consumedByKey: mObj.GetName()},
					},
					Data: existingSecret.Data,
				}
				err = r.GetClient().Create(context.TODO(), copiedSecret)
			} else if err == nil {
				existingCopiedSecret := copiedSecret.DeepCopyObject()
				if copiedSecret.Annotations == nil {
					copiedSecret.Annotations = map[string]string{}
				}
				copiedSecret.Annotations[consumedByKey] = AppendIfNotSubstring(mObj.GetName(), copiedSecret.Annotations[consumedByKey])
				copiedSecret.Data = existingSecret.Data
				// Skip setting the owner on the copiedSecret if the consumer and provider are in the same namespace
				// This is because we want the secret to be deleted if the provider is deleted
				if conNamespace != copiedSecret.Namespace {
					owner, _ := r.AsOwner(rObj, false)
					EnsureOwnerRef(copiedSecret, owner)
				}
				if !reflect.DeepEqual(existingCopiedSecret, copiedSecret) {
					err = r.GetClient().Update(context.TODO(), copiedSecret)
				}
			}

			if err != nil {
				return r.requeueError(ba, errors.Wrapf(err, "failed to create or update secret for consumes"))
			}

			consumedServices := ba.GetStatus().GetConsumedServices()
			if consumedServices == nil {
				consumedServices = common.ConsumedServices{}
			}
			if !ContainsString(consumedServices[common.ServiceBindingCategoryOpenAPI], secretName) {
				consumedServices[common.ServiceBindingCategoryOpenAPI] =
					append(consumedServices[common.ServiceBindingCategoryOpenAPI], secretName)
				ba.GetStatus().SetConsumedServices(consumedServices)
				err := r.UpdateStatus(rObj)
				if err != nil {
					return r.requeueError(ba, errors.Wrapf(err, "unable to update status with service binding secret information"))
				}
			}
		}
	}
	return r.done(ba)
}

// ReconcileBindings goes through the reconcile logic for service binding
func (r *ReconcilerBase) ReconcileBindings(ba common.BaseComponent) (reconcile.Result, error) {
	if ba.GetBindings() != nil && ba.GetBindings().GetEmbedded() != nil {
		if res, err := r.reconcileEmbedded(ba); isRequeue(res, err) {
			return res, err
		}
		return r.done(ba)
	}
	if err := r.cleanUpEmbeddedBindings(ba); err != nil {
		return r.requeueError(ba, err)
	}
	if res, err := r.reconcileExternals(ba); isRequeue(res, err) {
		return res, err
	}
	return r.done(ba)
}

func (r *ReconcilerBase) reconcileExternals(ba common.BaseComponent) (retRes reconcile.Result, retErr error) {
	mObj := ba.(metav1.Object)
	var resolvedBindings []string

	if ba.GetBindings() != nil && ba.GetBindings().GetResourceRef() != "" {
		bindingName := ba.GetBindings().GetResourceRef()
		key := types.NamespacedName{Name: bindingName, Namespace: mObj.GetNamespace()}
		bindingSecret := &corev1.Secret{}
		err := r.GetClient().Get(context.TODO(), key, bindingSecret)
		if err == nil {
			resolvedBindings = append(resolvedBindings, bindingName)
		} else {
			err = errors.Wrapf(err, "service binding dependency not satisfied: unable to find service binding secret for external binding %q in namespace %q", bindingName, mObj.GetNamespace())
			return r.requeueError(ba, err)
		}
	} else if ba.GetBindings() == nil || ba.GetBindings().GetAutoDetect() == nil || *ba.GetBindings().GetAutoDetect() {
		bindingName := getDefaultServiceBindingName(ba)
		key := types.NamespacedName{Name: bindingName, Namespace: mObj.GetNamespace()}

		for _, gvk := range r.getServiceBindingGVK() {
			// Using a unstructured object to find ServiceBinding CR since GVK might change
			bindingObj := &unstructured.Unstructured{}
			bindingObj.SetGroupVersionKind(gvk)
			err := r.client.Get(context.Background(), key, bindingObj)
			if err != nil {
				if !kerrors.IsNotFound(err) {
					log.Error(errors.Wrapf(err, "failed to find a service binding resource during auto-detect for GVK %q", gvk), "failed to get Service Binding CR")
				}
				continue
			}

			bindingSecret := &corev1.Secret{}
			err = r.GetClient().Get(context.TODO(), key, bindingSecret)
			if err == nil {
				resolvedBindings = append(resolvedBindings, bindingName)
				break
			} else {
				err = errors.Wrapf(err, "service binding dependency not satisfied: unable to find service binding secret for external binding %q in namespace %q", bindingName, mObj.GetNamespace())
				return r.requeueError(ba, err)
			}
		}
	}

	retRes, retErr = r.done(ba)
	defer func() {
		if res, err := r.updateBindingStatus(resolvedBindings, ba); isRequeue(res, err) {
			retRes, retErr = res, err
		}
	}()
	return
}

//GetResolvedBindingSecret returns the secret referenced in .status.resolvedBindings
func (r *ReconcilerBase) GetResolvedBindingSecret(ba common.BaseComponent) (*corev1.Secret, error) {
	if len(ba.GetStatus().GetResolvedBindings()) == 0 {
		return nil, nil
	}
	mObj := ba.(metav1.Object)
	secret := &corev1.Secret{}
	key := types.NamespacedName{Name: ba.GetStatus().GetResolvedBindings()[0], Namespace: mObj.GetNamespace()}
	err := r.client.Get(context.TODO(), key, secret)
	if err != nil {
		return nil, err
	}
	return secret, nil
}

// done when no error happens
func (r *ReconcilerBase) done(ba common.BaseComponent) (reconcile.Result, error) {
	return r.ManageSuccess(common.StatusConditionTypeDependenciesSatisfied, ba)
}

// requeueError simply calls ManageError when dependency is not fulfilled
func (r *ReconcilerBase) requeueError(ba common.BaseComponent, err error) (reconcile.Result, error) {
	r.ManageError(err, common.StatusConditionTypeDependenciesSatisfied, ba)
	return r.ManageError(errors.New("dependency not satisfied"), common.StatusConditionTypeReconciled, ba)
}

func isRequeue(res reconcile.Result, err error) bool {
	return err != nil || res.Requeue
}

func getCopiedToNamespacesAnnotationKey(ba common.BaseComponent) string {
	return "service." + ba.GetGroupName() + "/copied-to-namespaces"
}

func getConsumedByAnnotationKey(ba common.BaseComponent) string {
	return "service." + ba.GetGroupName() + "/consumed-by"
}

func equals(sl1, sl2 []string) bool {
	if len(sl1) != len(sl2) {
		return false
	}
	for i, v := range sl1 {
		if v != sl2[i] {
			return false
		}
	}
	return true
}

func getOpConfigServiceBindingGVKs() []schema.GroupVersionKind {
	gvkStringList := strings.Split(common.Config[common.OpConfigSvcBindingGVKs], ",")
	for i := range gvkStringList {
		gvkStringList[i] = strings.TrimSpace(gvkStringList[i])
	}

	gvkList := []schema.GroupVersionKind{}
	for i := range gvkStringList {
		gvk, _ := schema.ParseKindArg(gvkStringList[i])
		if gvk == nil {
			log.Error(errors.Errorf("failed to parse %q to a valid GroupVersionKind", gvkStringList[i]), "Invalid GroupVersionKind from operator ConfigMap")
			continue
		}
		gvkList = append(gvkList, *gvk)
	}
	return gvkList
}

func (r *ReconcilerBase) getServiceBindingGVK() (gvkList []schema.GroupVersionKind) {
	for _, gvk := range getOpConfigServiceBindingGVKs() {
		if ok, _ := r.IsGroupVersionSupported(gvk.GroupVersion().String(), gvk.Kind); ok {
			gvkList = append(gvkList, gvk)
		}
	}
	return gvkList
}

// IsServiceBindingSupported returns true if at least one GVK in the operator ConfigMap's serviceBinding.groupVersionKinds is installed
func (r *ReconcilerBase) IsServiceBindingSupported() bool {
	return len(r.getServiceBindingGVK()) > 0
}

// cleanUpEmbeddedBindings deletes Service Binding resources owned by current CR based having the same name as CR
func (r *ReconcilerBase) cleanUpEmbeddedBindings(ba common.BaseComponent) error {
	mObj := ba.(metav1.Object)
	for _, gvk := range r.getServiceBindingGVK() {
		bindingObj := &unstructured.Unstructured{}
		bindingObj.SetGroupVersionKind(gvk)
		key := types.NamespacedName{Name: getDefaultServiceBindingName(ba), Namespace: mObj.GetNamespace()}
		err := r.client.Get(context.Background(), key, bindingObj)
		if err == nil && metav1.IsControlledBy(bindingObj, mObj) {
			err = r.client.Delete(context.Background(), bindingObj)
			if client.IgnoreNotFound(err) != nil {
				return err
			}
		} else if client.IgnoreNotFound(err) != nil {
			return err
		}
	}
	return nil
}

// reconcileEmbedded reconciles embedded blob in bindings.embedded to create or update Service Binding resource
func (r *ReconcilerBase) reconcileEmbedded(ba common.BaseComponent) (retRes reconcile.Result, retErr error) {
	var resolvedBindings []string

	object, err := r.toJSONFromRaw(ba.GetBindings().GetEmbedded())
	if err != nil {
		err = errors.Wrapf(err, "failed: unable marshalling to JSON")
		return r.requeueError(ba, err)
	}

	embedded := &unstructured.Unstructured{}
	embedded.SetUnstructuredContent(object)
	err = r.updateEmbeddedObject(object, embedded, ba)
	if err != nil {
		err = errors.Wrapf(err, "failed: cannot add missing information to the embedded Service Binding")
		return r.requeueError(ba, err)
	}

	apiVersion, kind := embedded.GetAPIVersion(), embedded.GetKind()
	ok, err := r.IsGroupVersionSupported(apiVersion, kind)
	if !ok {
		err = errors.Wrapf(err, "failed: embedded Service Binding CRD with GroupVersion %q and Kind %q is not supported on the cluster", apiVersion, kind)
		return r.requeueError(ba, err)
	}

	err = r.createOrUpdateEmbedded(embedded, ba)
	if err != nil {
		return r.requeueError(ba, errors.Wrapf(err, "failed: cannot create or update embedded Service Binding resource %q in namespace %q", embedded.GetName(), embedded.GetNamespace()))
	}

	// Get binding secret and add it to status field. If binding hasn't been successful, secret won't be created and it keeps trying
	key := types.NamespacedName{Name: embedded.GetName(), Namespace: embedded.GetNamespace()}
	bindingSecret := &corev1.Secret{}
	err = r.GetClient().Get(context.TODO(), key, bindingSecret)
	if err == nil {
		resolvedBindings = append(resolvedBindings, embedded.GetName())
	} else {
		err = errors.Wrapf(err, "service binding dependency not satisfied: unable to find service binding secret for embedded binding %q in namespace %q", embedded.GetName(), embedded.GetNamespace())
		return r.requeueError(ba, err)
	}

	retRes, retErr = r.done(ba)
	defer func() {
		if res, err := r.updateBindingStatus(resolvedBindings, ba); isRequeue(res, err) {
			retRes, retErr = res, err
		}
	}()
	return
}

func (r *ReconcilerBase) updateBindingStatus(bindings []string, ba common.BaseComponent) (reconcile.Result, error) {
	if !equals(bindings, ba.GetStatus().GetResolvedBindings()) {
		sort.Strings(bindings)
		ba.GetStatus().SetResolvedBindings(bindings)
		if err := r.UpdateStatus(ba.(runtime.Object)); err != nil {
			return r.requeueError(ba, errors.Wrapf(err, "unable to update status with resolved service binding information"))
		}
	}
	return r.done(ba)
}

func (r *ReconcilerBase) createOrUpdateEmbedded(embedded *unstructured.Unstructured, ba common.BaseComponent) error {
	result := controllerutil.OperationResultNone
	existing := &unstructured.Unstructured{}
	existing.SetAPIVersion(embedded.GetAPIVersion())
	existing.SetKind(embedded.GetKind())
	key := types.NamespacedName{Name: embedded.GetName(), Namespace: embedded.GetNamespace()}
	err := r.client.Get(context.TODO(), key, existing)
	if err != nil {
		if kerrors.IsNotFound(err) {
			err = r.client.Create(context.TODO(), embedded)
			if err != nil {
				return err
			}
			result = controllerutil.OperationResultCreated
			// add watcher
			err = r.controller.Watch(&source.Kind{Type: existing}, &handler.EnqueueRequestForOwner{
				IsController: true,
				OwnerType:    ba.(runtime.Object),
			})
			if err != nil {
				return errors.Wrap(err, "Cannot add watcher")
			}
		} else {
			return err
		}
	} else {
		// Update the found object and write the result back if there are any changes
		if !reflect.DeepEqual(embedded.Object[Spec], existing.Object[Spec]) {
			existing.Object[Spec] = embedded.Object[Spec]
			err = r.client.Update(context.TODO(), existing)
			if err != nil {
				return err
			}
			result = controllerutil.OperationResultUpdated
		}
	}

	log.Info("Reconciled", "Kind", embedded.GetKind(), "Namespace", embedded.GetNamespace(), "Name", embedded.GetName(), "Status", result)
	return nil
}

func (r *ReconcilerBase) toJSONFromRaw(content *runtime.RawExtension) (map[string]interface{}, error) {
	var data map[string]interface{}
	if err := json.Unmarshal(content.Raw, &data); err != nil {
		return nil, err
	}
	return data, nil
}

func (r *ReconcilerBase) updateEmbeddedObject(object map[string]interface{}, embedded *unstructured.Unstructured, ba common.BaseComponent) error {
	mObj := ba.(metav1.Object)

	if _, ok := object[Spec]; !ok {
		return errors.New("failed: embedded Service Binding is missing a 'spec' section")
	}

	if _, ok := object[Metadata]; ok {
		return errors.New("failed: embedded Service Binding must not have a 'metadata' section")
	}
	embedded.SetName(getDefaultServiceBindingName(ba))
	embedded.SetNamespace(mObj.GetNamespace())
	embedded.SetLabels(mObj.GetLabels())
	embedded.SetAnnotations(mObj.GetAnnotations())

	if err := controllerutil.SetControllerReference(mObj, embedded, r.scheme); err != nil {
		return errors.Wrap(err, "SetControllerReference returned error")
	}

	apiVersion, okAPIVersion := object[APIVersion]
	kind, okKind := object[Kind]

	// If either API Version or Kind is not set, try getting it from the Operator ConfigMap
	var defaultGVK schema.GroupVersionKind
	if !okAPIVersion || !okKind {
		cmGVK := getOpConfigServiceBindingGVKs()
		if len(cmGVK) == 0 {
			return errors.New("failed: embedded Service Binding does not specify 'apiVersion' or 'kind' and there is no default GVK defined in the operator ConfigMap")
		}
		defaultGVK = cmGVK[0]
	}
	if !okAPIVersion {
		apiVersion = defaultGVK.GroupVersion().String()
	}
	if !okKind {
		kind = defaultGVK.Kind
	}

	embedded.SetKind(kind.(string))
	embedded.SetAPIVersion(apiVersion.(string))
	return nil
}

func getDefaultServiceBindingName(ba common.BaseComponent) string {
	return (ba.(metav1.Object)).GetName() + "-binding"
}
