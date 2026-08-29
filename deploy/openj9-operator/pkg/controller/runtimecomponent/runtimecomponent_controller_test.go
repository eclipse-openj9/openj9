package runtimecomponent

import (
	"context"
	"os"
	"strconv"
	"testing"

	prometheusv1 "github.com/coreos/prometheus-operator/pkg/apis/monitoring/v1"
	certmngrv1alpha2 "github.com/jetstack/cert-manager/pkg/apis/certmanager/v1alpha2"
	openj9v1beta1 "github.com/eclipse/openj9/pkg/apis/openj9/v1beta1"
	openj9utils "github.com/eclipse/openj9/pkg/utils"

	servingv1alpha1 "github.com/knative/serving/pkg/apis/serving/v1alpha1"
	imagev1 "github.com/openshift/api/image/v1"
	routev1 "github.com/openshift/api/route/v1"
	appsv1 "k8s.io/api/apps/v1"
	autoscalingv1 "k8s.io/api/autoscaling/v1"
	corev1 "k8s.io/api/core/v1"
	metav1 "k8s.io/apimachinery/pkg/apis/meta/v1"
	"k8s.io/apimachinery/pkg/runtime"
	"k8s.io/apimachinery/pkg/types"
	"k8s.io/apimachinery/pkg/util/intstr"
	"k8s.io/client-go/discovery"
	fakediscovery "k8s.io/client-go/discovery/fake"
	"k8s.io/client-go/kubernetes/scheme"
	"k8s.io/client-go/rest"
	coretesting "k8s.io/client-go/testing"
	"k8s.io/client-go/tools/record"
	applicationsv1beta1 "sigs.k8s.io/application/pkg/apis/app/v1beta1"
	fakeclient "sigs.k8s.io/controller-runtime/pkg/client/fake"
	"sigs.k8s.io/controller-runtime/pkg/reconcile"
	logf "sigs.k8s.io/controller-runtime/pkg/runtime/log"
)

var (
	name                       = "app"
	namespace                  = "runtimecomponent"
	appImage                   = "my-image"
	ksvcAppImage               = "ksvc-image"
	replicas             int32 = 3
	autoscaling                = &openj9v1beta1.RuntimeComponentAutoScaling{MaxReplicas: 3}
	pullPolicy                 = corev1.PullAlways
	serviceType                = corev1.ServiceTypeClusterIP
	service                    = &openj9v1beta1.RuntimeComponentService{Type: &serviceType, Port: 8080}
	expose                     = true
	serviceAccountName         = "service-account"
	volumeCT                   = &corev1.PersistentVolumeClaim{TypeMeta: metav1.TypeMeta{Kind: "StatefulSet"}}
	storage                    = openj9v1beta1.RuntimeComponentStorage{Size: "10Mi", MountPath: "/mnt/data", VolumeClaimTemplate: volumeCT}
	createKnativeService       = true
	statefulSetSN              = name + "-headless"
)

type Test struct {
	test     string
	expected interface{}
	actual   interface{}
}

func TestRuntimeController(t *testing.T) {
	// Set the logger to development mode for verbose logs
	logf.SetLogger(logf.ZapLogger(true))
	os.Setenv("WATCH_NAMESPACE", namespace)

	spec := openj9v1beta1.RuntimeComponentSpec{}
	runtimecomponent := createRuntimeComponent(name, namespace, spec)

	// Set objects to track in the fake client and register operator types with the runtime scheme.
	objs, s := []runtime.Object{runtimecomponent}, scheme.Scheme

	// Add third party resrouces to scheme
	if err := servingv1alpha1.AddToScheme(s); err != nil {
		t.Fatalf("Unable to add servingv1alpha1 scheme: (%v)", err)
	}

	if err := routev1.AddToScheme(s); err != nil {
		t.Fatalf("Unable to add route scheme: (%v)", err)
	}

	if err := imagev1.AddToScheme(s); err != nil {
		t.Fatalf("Unable to add image scheme: (%v)", err)
	}

	if err := applicationsv1beta1.AddToScheme(s); err != nil {
		t.Fatalf("Unable to add image scheme: (%v)", err)
	}

	if err := certmngrv1alpha2.AddToScheme(s); err != nil {
		t.Fatalf("Unable to add cert-manager scheme: (%v)", err)
	}

	if err := prometheusv1.AddToScheme(s); err != nil {
		t.Fatalf("Unable to add prometheus scheme: (%v)", err)
	}

	s.AddKnownTypes(openj9v1beta1.SchemeGroupVersion, runtimecomponent)
	s.AddKnownTypes(certmngrv1alpha2.SchemeGroupVersion, &certmngrv1alpha2.Certificate{})
	s.AddKnownTypes(prometheusv1.SchemeGroupVersion, &prometheusv1.ServiceMonitor{})

	// Create a fake client to mock API calls.
	cl := fakeclient.NewFakeClient(objs...)

	rb := openj9utils.NewReconcilerBase(cl, s, &rest.Config{}, record.NewFakeRecorder(10))

	// Create a ReconcileRuntimeComponent object
	r := &ReconcileRuntimeComponent{ReconcilerBase: rb}
	r.SetDiscoveryClient(createFakeDiscoveryClient())

	// Mock request to simulate Reconcile being called on an event for a watched resource
	// then ensure reconcile is successful and does not return an empty result
	req := createReconcileRequest(name, namespace)
	res, err := r.Reconcile(req)
	verifyReconcile(res, err, t)

	// Check if deployment has been created
	dep := &appsv1.Deployment{}
	if err = r.GetClient().Get(context.TODO(), req.NamespacedName, dep); err != nil {
		t.Fatalf("Get Deployment: (%v)", err)
	}

	depTests := []Test{
		{"service account name", "app", dep.Spec.Template.Spec.ServiceAccountName},
	}
	verifyTests("dep", depTests, t)

	// Update runtimecomponentwith values for StatefulSet
	// Update ServiceAccountName for empty case
	runtimecomponent.Spec = openj9v1beta1.RuntimeComponentSpec{
		Storage:          &storage,
		Replicas:         &replicas,
		ApplicationImage: appImage,
	}
	updateRuntimeComponent(r, runtimecomponent, t)

	// Reconcile again to check for the StatefulSet and updated resources
	res, err = r.Reconcile(req)
	verifyReconcile(res, err, t)

	// Check if StatefulSet has been created
	statefulSet := &appsv1.StatefulSet{}
	if err = r.GetClient().Get(context.TODO(), req.NamespacedName, statefulSet); err != nil {
		t.Fatalf("Get StatefulSet: (%v)", err)
	}

	// Storage is enabled so the deployment should be deleted
	if err = r.GetClient().Get(context.TODO(), req.NamespacedName, dep); err == nil {
		t.Fatalf("Deployment was not deleted")
	}

	// Check updated values in StatefulSet
	ssTests := []Test{
		{"replicas", replicas, *statefulSet.Spec.Replicas},
		{"service image name", appImage, statefulSet.Spec.Template.Spec.Containers[0].Image},
		{"pull policy", name, statefulSet.Spec.Template.Spec.ServiceAccountName},
		{"service account name", statefulSetSN, statefulSet.Spec.ServiceName},
	}
	verifyTests("statefulSet", ssTests, t)

	// Enable CreateKnativeService
	runtimecomponent.Spec = openj9v1beta1.RuntimeComponentSpec{
		CreateKnativeService: &createKnativeService,
		PullPolicy:           &pullPolicy,
		ApplicationImage:     ksvcAppImage,
	}
	updateRuntimeComponent(r, runtimecomponent, t)

	// Reconcile again to check for the KNativeService and updated resources
	res, err = r.Reconcile(req)
	verifyReconcile(res, err, t)

	// Create KnativeService
	ksvc := &servingv1alpha1.Service{
		TypeMeta: metav1.TypeMeta{
			APIVersion: "serving.knative.dev/v1alpha1",
			Kind:       "Service",
		},
	}
	if err = r.GetClient().Get(context.TODO(), req.NamespacedName, ksvc); err != nil {
		t.Fatalf("Get KnativeService: (%v)", err)
	}

	// KnativeService is enabled so non-Knative resources should be deleted
	if err = r.GetClient().Get(context.TODO(), req.NamespacedName, statefulSet); err == nil {
		t.Fatalf("StatefulSet was not deleted")
	}

	// Check updated values in KnativeService
	ksvcTests := []Test{
		{"service image name", ksvcAppImage, ksvc.Spec.Template.Spec.Containers[0].Image},
		{"pull policy", pullPolicy, ksvc.Spec.Template.Spec.Containers[0].ImagePullPolicy},
		{"service account name", name, ksvc.Spec.Template.Spec.ServiceAccountName},
	}
	verifyTests("ksvc", ksvcTests, t)

	// Disable Knative and enable Expose to test route
	runtimecomponent.Spec = openj9v1beta1.RuntimeComponentSpec{Expose: &expose}
	updateRuntimeComponent(r, runtimecomponent, t)

	// Reconcile again to check for the route and updated resources
	res, err = r.Reconcile(req)
	verifyReconcile(res, err, t)

	// Create Route
	route := &routev1.Route{
		TypeMeta: metav1.TypeMeta{APIVersion: "route.openshift.io/v1", Kind: "Route"},
	}
	if err = r.GetClient().Get(context.TODO(), req.NamespacedName, route); err != nil {
		t.Fatalf("Get Route: (%v)", err)
	}

	// Check updated values in Route
	routeTests := []Test{{"target port", intstr.FromString(strconv.Itoa(int(service.Port)) + "-tcp"), route.Spec.Port.TargetPort}}
	verifyTests("route", routeTests, t)

	// Disable Route/Expose and enable Autoscaling
	runtimecomponent.Spec = openj9v1beta1.RuntimeComponentSpec{
		Autoscaling: autoscaling,
	}
	updateRuntimeComponent(r, runtimecomponent, t)

	// Reconcile again to check for hpa and updated resources
	res, err = r.Reconcile(req)
	verifyReconcile(res, err, t)

	// Create HorizontalPodAutoscaler
	hpa := &autoscalingv1.HorizontalPodAutoscaler{}
	if err = r.GetClient().Get(context.TODO(), req.NamespacedName, hpa); err != nil {
		t.Fatalf("Get HPA: (%v)", err)
	}

	// Expose is disabled so route should be deleted
	if err = r.GetClient().Get(context.TODO(), req.NamespacedName, route); err == nil {
		t.Fatal("Route was not deleted")
	}

	// Check updated values in hpa
	hpaTests := []Test{{"max replicas", autoscaling.MaxReplicas, hpa.Spec.MaxReplicas}}
	verifyTests("hpa", hpaTests, t)

	// Remove autoscaling to ensure hpa is deleted
	runtimecomponent.Spec.Autoscaling = nil
	updateRuntimeComponent(r, runtimecomponent, t)

	res, err = r.Reconcile(req)
	verifyReconcile(res, err, t)

	// Autoscaling is disabled so hpa should be deleted
	if err = r.GetClient().Get(context.TODO(), req.NamespacedName, hpa); err == nil {
		t.Fatal("hpa was not deleted")
	}

	if err = r.GetClient().Get(context.TODO(), req.NamespacedName, runtimecomponent); err != nil {
		t.Fatalf("Get runtimecomponent: (%v)", err)
	}

	// Update runtimecomponentto ensure it requeues
	runtimecomponent.SetGeneration(1)
	updateRuntimeComponent(r, runtimecomponent, t)

	res, err = r.Reconcile(req)
	if err != nil {
		t.Fatalf("reconcile: (%v)", err)
	}
}

// Helper Functions
func createRuntimeComponent(n, ns string, spec openj9v1beta1.RuntimeComponentSpec) *openj9v1beta1.RuntimeComponent {
	app := &openj9v1beta1.RuntimeComponent{
		ObjectMeta: metav1.ObjectMeta{Name: n, Namespace: ns},
		Spec:       spec,
	}
	return app
}

func createFakeDiscoveryClient() discovery.DiscoveryInterface {
	fakeDiscoveryClient := &fakediscovery.FakeDiscovery{Fake: &coretesting.Fake{}}
	fakeDiscoveryClient.Resources = []*metav1.APIResourceList{
		{
			GroupVersion: routev1.SchemeGroupVersion.String(),
			APIResources: []metav1.APIResource{
				{Name: "routes", Namespaced: true, Kind: "Route"},
			},
		},
		{
			GroupVersion: servingv1alpha1.SchemeGroupVersion.String(),
			APIResources: []metav1.APIResource{
				{Name: "services", Namespaced: true, Kind: "Service", SingularName: "service"},
			},
		},
		{
			GroupVersion: certmngrv1alpha2.SchemeGroupVersion.String(),
			APIResources: []metav1.APIResource{
				{Name: "certificates", Namespaced: true, Kind: "Certificate", SingularName: "certificate"},
			},
		},
		{
			GroupVersion: prometheusv1.SchemeGroupVersion.String(),
			APIResources: []metav1.APIResource{
				{Name: "servicemonitors", Namespaced: true, Kind: "ServiceMonitor", SingularName: "servicemonitor"},
			},
		},
		{
			GroupVersion: imagev1.SchemeGroupVersion.String(),
			APIResources: []metav1.APIResource{
				{Name: "imagestreams", Namespaced: true, Kind: "ImageStream", SingularName: "imagestream"},
			},
		},
		{
			GroupVersion: applicationsv1beta1.SchemeGroupVersion.String(),
			APIResources: []metav1.APIResource{
				{Name: "applications", Namespaced: true, Kind: "Application", SingularName: "application"},
			},
		},
	}

	return fakeDiscoveryClient
}

func createReconcileRequest(n, ns string) reconcile.Request {
	req := reconcile.Request{
		NamespacedName: types.NamespacedName{Name: n, Namespace: ns},
	}
	return req
}

func createConfigMap(n, ns string, data map[string]string) *corev1.ConfigMap {
	app := &corev1.ConfigMap{
		ObjectMeta: metav1.ObjectMeta{Name: n, Namespace: ns},
		Data:       data,
	}
	return app
}

func verifyReconcile(res reconcile.Result, err error, t *testing.T) {
	if err != nil {
		t.Fatalf("reconcile: (%v)", err)
	}

	if res != (reconcile.Result{}) {
		t.Errorf("reconcile did not return an empty result (%v)", res)
	}
}

func verifyTests(n string, tests []Test, t *testing.T) {
	for _, tt := range tests {
		if tt.actual != tt.expected {
			t.Errorf("%s %s test expected: (%v) actual: (%v)", n, tt.test, tt.expected, tt.actual)
		}
	}
}

func updateRuntimeComponent(r *ReconcileRuntimeComponent, runtimecomponent *openj9v1beta1.RuntimeComponent, t *testing.T) {
	if err := r.GetClient().Update(context.TODO(), runtimecomponent); err != nil {
		t.Fatalf("Update runtimecomponent: (%v)", err)
	}
}
