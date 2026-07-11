package runtimecomponent

import (
	"context"
	"strings"

	openj9v1beta1 "github.com/eclipse/openj9/pkg/apis/openj9/v1beta1"
	openj9utils "github.com/eclipse/openj9/pkg/utils"
	corev1 "k8s.io/api/core/v1"
	kerrors "k8s.io/apimachinery/pkg/api/errors"
	metav1 "k8s.io/apimachinery/pkg/apis/meta/v1"
	"k8s.io/apimachinery/pkg/runtime"
	"k8s.io/apimachinery/pkg/types"
	"k8s.io/client-go/util/workqueue"
	"sigs.k8s.io/controller-runtime/pkg/client"
	"sigs.k8s.io/controller-runtime/pkg/event"
	"sigs.k8s.io/controller-runtime/pkg/handler"
	"sigs.k8s.io/controller-runtime/pkg/reconcile"
)

var _ handler.EventHandler = &EnqueueRequestsForCustomIndexField{}

const (
	indexFieldImageStreamName     = "spec.applicationImage"
	indexFieldBindingsResourceRef = "spec.bindings.resourceRef"
	bindingSecretSuffix           = "-binding"
)

// EnqueueRequestsForCustomIndexField enqueues reconcile Requests Runtime Components if the app is relying on
// the modified resource
type EnqueueRequestsForCustomIndexField struct {
	handler.Funcs
	Matcher CustomMatcher
}

// Update implements EventHandler
func (e *EnqueueRequestsForCustomIndexField) Update(evt event.UpdateEvent, q workqueue.RateLimitingInterface) {
	e.handle(evt.MetaNew, evt.ObjectNew, q)
}

// Delete implements EventHandler
func (e *EnqueueRequestsForCustomIndexField) Delete(evt event.DeleteEvent, q workqueue.RateLimitingInterface) {
	e.handle(evt.Meta, evt.Object, q)
}

// Generic implements EventHandler
func (e *EnqueueRequestsForCustomIndexField) Generic(evt event.GenericEvent, q workqueue.RateLimitingInterface) {
	e.handle(evt.Meta, evt.Object, q)
}

// handle common implementation to enqueue reconcile Requests for applications
func (e *EnqueueRequestsForCustomIndexField) handle(evtMeta metav1.Object, evtObj runtime.Object, q workqueue.RateLimitingInterface) {
	apps, _ := e.Matcher.Match(evtMeta)
	for _, app := range apps {
		q.Add(reconcile.Request{
			NamespacedName: types.NamespacedName{
				Namespace: app.Namespace,
				Name:      app.Name,
			}})
	}
}

// CustomMatcher is an interface for matching apps that satisfy a custom logic
type CustomMatcher interface {
	Match(metav1.Object) ([]openj9v1beta1.RuntimeComponent, error)
}

// ImageStreamMatcher implements CustomMatcher for Image Streams
type ImageStreamMatcher struct {
	Klient          client.Client
	WatchNamespaces []string
}

// Match returns all applications using the input ImageStreamTag
func (i *ImageStreamMatcher) Match(imageStreamTag metav1.Object) ([]openj9v1beta1.RuntimeComponent, error) {
	apps := []openj9v1beta1.RuntimeComponent{}
	var namespaces []string
	if openj9utils.IsClusterWide(i.WatchNamespaces) {
		nsList := &corev1.NamespaceList{}
		if err := i.Klient.List(context.Background(), nsList, client.InNamespace("")); err != nil {
			return nil, err
		}
		for _, ns := range nsList.Items {
			namespaces = append(namespaces, ns.Name)
		}
	} else {
		namespaces = i.WatchNamespaces
	}
	for _, ns := range namespaces {
		appList := &openj9v1beta1.RuntimeComponentList{}
		err := i.Klient.List(context.Background(),
			appList,
			client.InNamespace(ns),
			client.MatchingFields{indexFieldImageStreamName: imageStreamTag.GetNamespace() + "/" + imageStreamTag.GetName()})
		if err != nil {
			return nil, err
		}
		apps = append(apps, appList.Items...)
	}

	return apps, nil
}

// BindingSecretMatcher implements CustomMatcher for Binding Secrets
type BindingSecretMatcher struct {
	klient client.Client
}

// Match returns all applications that "could" rely on the secret as a secret binding by finding apps that have
// resourceRef matching the secret name OR app name matching the secret name
func (b *BindingSecretMatcher) Match(secret metav1.Object) ([]openj9v1beta1.RuntimeComponent, error) {
	apps := []openj9v1beta1.RuntimeComponent{}

	// Adding apps which have this secret defined in the spec.bindings.resourceRef
	appList := &openj9v1beta1.RuntimeComponentList{}
	err := b.klient.List(context.Background(),
		appList,
		client.InNamespace(secret.GetNamespace()),
		client.MatchingFields{indexFieldBindingsResourceRef: secret.GetName()})
	if err != nil {
		return nil, err
	}
	apps = append(apps, appList.Items...)

	if strings.HasSuffix(secret.GetName(), bindingSecretSuffix) {
		appName := strings.TrimSuffix(secret.GetName(), bindingSecretSuffix)
		// If we are able to find an app with the secret name, add the app. This is to cover the autoDetect scenario
		app := &openj9v1beta1.RuntimeComponent{}
		err = b.klient.Get(context.Background(), types.NamespacedName{Name: appName, Namespace: secret.GetNamespace()}, app)
		if err == nil {
			apps = append(apps, *app)
		} else if !kerrors.IsNotFound(err) {
			return nil, err
		}
	}

	return apps, nil
}
