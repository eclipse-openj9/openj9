package utils

import (
	"context"
	"strings"

	corev1 "k8s.io/api/core/v1"
	"k8s.io/apimachinery/pkg/api/errors"
	metav1 "k8s.io/apimachinery/pkg/apis/meta/v1"
	"k8s.io/apimachinery/pkg/types"
	"k8s.io/client-go/util/workqueue"
	"sigs.k8s.io/controller-runtime/pkg/client"
	"sigs.k8s.io/controller-runtime/pkg/event"
	"sigs.k8s.io/controller-runtime/pkg/handler"
	"sigs.k8s.io/controller-runtime/pkg/reconcile"
)

// EnqueueRequestsForServiceBinding enqueues reconcile Requests for applications affected by the secrets that
// EventHandler is called for
type EnqueueRequestsForServiceBinding struct {
	handler.Funcs
	WatchNamespaces []string
	GroupName       string
	Client          client.Client
}

// Update implements EventHandler
func (e *EnqueueRequestsForServiceBinding) Update(evt event.UpdateEvent, q workqueue.RateLimitingInterface) {
	e.handle(evt.MetaNew, q)
}

// Delete implements EventHandler
func (e *EnqueueRequestsForServiceBinding) Delete(evt event.DeleteEvent, q workqueue.RateLimitingInterface) {
	e.handle(evt.Meta, q)
}

// Generic implements EventHandler
func (e *EnqueueRequestsForServiceBinding) Generic(evt event.GenericEvent, q workqueue.RateLimitingInterface) {
	e.handle(evt.Meta, q)
}

// handle common implementation to enqueue reconcile Requests for applications affected by the secrets that
// EventHandler is called for
func (e *EnqueueRequestsForServiceBinding) handle(evtMeta metav1.Object, q workqueue.RateLimitingInterface) {
	if evtMeta.GetLabels() == nil || evtMeta.GetLabels()["service."+e.GroupName+"/bindable"] != "true" {
		return
	}

	apps, _ := e.matchApplication(evtMeta)
	for _, app := range apps {
		q.Add(reconcile.Request{
			NamespacedName: types.NamespacedName{
				Namespace: app.Namespace,
				Name:      app.Name,
			}})
	}
}

// matchApplication returns the NamespacedName of all applications consuming (directly or indirectly) the service
// binding secret
func (e *EnqueueRequestsForServiceBinding) matchApplication(mSecret metav1.Object) ([]types.NamespacedName, error) {
	allSecrets, err := e.getDependentSecrets(mSecret.GetName())
	if err != nil {
		return nil, err
	}

	// Manually add the secret currenly being handled. This is needed as the secret might have been deleted already
	tmpSecret := corev1.Secret{ObjectMeta: metav1.ObjectMeta{Name: mSecret.GetName(), Namespace: mSecret.GetNamespace(), Annotations: mSecret.GetAnnotations()}}
	allSecrets = append(allSecrets, tmpSecret)

	matched := []types.NamespacedName{}
	for _, secret := range allSecrets {
		if secret.Annotations != nil {
			if consumedBy, ok := secret.Annotations["service."+e.GroupName+"/consumed-by"]; ok {
				for _, app := range strings.Split(consumedBy, ",") {
					matched = append(matched, types.NamespacedName{Name: app, Namespace: secret.Namespace})
				}
			}
		}
	}
	return matched, nil
}

// getDependentSecrets returns all the secrets with the input name in the namespaces the operator is watching.
func (e *EnqueueRequestsForServiceBinding) getDependentSecrets(secretName string) ([]corev1.Secret, error) {
	dependents := []corev1.Secret{}
	var namespaces []string

	if IsClusterWide(e.WatchNamespaces) {
		nsList := &corev1.NamespaceList{}
		err := e.Client.List(context.Background(), nsList, client.InNamespace(""))
		if err != nil {
			return nil, err
		}
		for _, ns := range nsList.Items {
			namespaces = append(namespaces, ns.Name)
		}
	} else {
		namespaces = e.WatchNamespaces
	}

	for _, ns := range namespaces {
		depSecret := &corev1.Secret{}
		err := e.Client.Get(context.Background(), client.ObjectKey{Name: secretName, Namespace: ns}, depSecret)
		if err != nil && !errors.IsNotFound(err) {
			return nil, err
		}
		dependents = append(dependents, *depSecret)
	}

	return dependents, nil
}
